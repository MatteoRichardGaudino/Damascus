/*______________________________________________________________________________
  MCTS_PUCT Italian Checkers Engine (Dama Italiana)
  High-Performance Monte Carlo Tree Search with PUCT & Prior Domain Heuristics
  Zero dynamic allocations during search, Anytime budget compliance, Subtree reuse
______________________________________________________________________________*/

#include "mcts_puct.h"
#include "wld_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_TREE_DEPTH 256

typedef struct {
    double   time_budget;
    float    c_puct;
    float    temperature;
    int      max_rollout_depth;
    bool     use_db;
    bool     debug_log;
    uint32_t root_idx;
    uint32_t rng_state;
    bool     has_prev_state;
    GameState prev_game_state;
    Move     prev_ai_move;
} PUCTEngineState;

// Static Memory Pool of 2,000,000 nodes (~48 MB in BSS)
static PUCTNode s_node_pool[PUCT_MAX_NODES];
static uint32_t s_pool_tail = 0;

static inline double puct_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0x85431249U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void pool_reset(void) {
    s_pool_tail = 0;
}

static uint32_t create_root_node(void) {
    if (s_pool_tail >= PUCT_MAX_NODES) {
        pool_reset();
    }
    uint32_t idx = s_pool_tail++;
    s_node_pool[idx].visits = 0;
    s_node_pool[idx].wins = 0.0f;
    s_node_pool[idx].prior = 1.0f;
    s_node_pool[idx].parent_idx = UINT32_MAX;
    s_node_pool[idx].first_child_idx = UINT32_MAX;
    s_node_pool[idx].move = MOVE_NONE;
    s_node_pool[idx].num_children = 0;
    s_node_pool[idx].unexpanded_idx = 0;
    return idx;
}

/* Fast Domain Heuristic H(s, a) for Italian Draughts:
   - King capture: +3.0
   - Man capture: +1.5
   - Promotion move: +2.0
   - King move: +0.5
   - Advancement towards promotion rank: +0.2 * Delta_row
   - Moving away from base back-rank defense: -0.3
*/
float puct_compute_heuristic(const GameState *state, Move move) {
    if (move_is_none(move)) return 0.0f;

    Player player = state->current_player;
    float h = 0.0f;

    // 1. Captures Evaluation (King vs Man)
    if (move.is_cap) {
        uint32_t opp_kings = (player == PLAYER_WHITE) ? state->board.black_kings : state->board.white_kings;
        uint32_t opp_men   = (player == PLAYER_WHITE) ? state->board.black_men : state->board.white_men;

        if (move.jumps > 0) {
            for (int j = 0; j < move.jumps; j++) {
                uint8_t cap_sq = move.caps[j];
                if (cap_sq < 32) {
                    uint32_t mask = 1U << cap_sq;
                    if (opp_kings & mask) {
                        h += 3.0f;
                    } else if (opp_men & mask) {
                        h += 1.5f;
                    } else {
                        h += 1.5f; // Fallback man capture
                    }
                }
            }
        } else if (move.cap_mask != 0) {
            uint32_t mask = move.cap_mask;
            while (mask) {
                int cap_sq = __builtin_ctz(mask);
                mask &= mask - 1;
                uint32_t sq_bit = 1U << cap_sq;
                if (opp_kings & sq_bit) {
                    h += 3.0f;
                } else {
                    h += 1.5f;
                }
            }
        } else {
            h += 1.5f;
        }
    }

    // 2. Promotion move: +2.0
    if (MOVE_IS_PROM(move)) {
        h += 2.0f;
    }

    // 3. King move: +0.5
    if (move.piece_type == 1) {
        h += 0.5f;
    }

    // 4. Advancement towards promotion rank (for Men): +0.2 * Delta_row
    int from_row = SQ_TO_ROW(move.from);
    int to_row   = SQ_TO_ROW(move.to);
    if (move.piece_type == 0) {
        int delta_row = (player == PLAYER_WHITE) ? (to_row - from_row) : (from_row - to_row);
        if (delta_row > 0) {
            h += 0.2f * (float)delta_row;
        }
    }

    // 5. Moving away from base back-rank defense: -0.3
    int base_row = (player == PLAYER_WHITE) ? 0 : 7;
    if (from_row == base_row) {
        h -= 0.3f;
    }

    return h;
}

// Compute Softmax Prior Policy P(s, a) = exp(H(s, a) / tau) / sum(exp(H(s, b) / tau))
static void compute_priors(const GameState *state, const MoveList *moves, float tau, float *priors) {
    if (moves->count == 0) return;
    if (moves->count == 1) {
        priors[0] = 1.0f;
        return;
    }

    if (tau <= 0.001f) tau = 1.0f;

    float h_scores[48];
    float max_h = -1e9f;

    for (int i = 0; i < moves->count; i++) {
        h_scores[i] = puct_compute_heuristic(state, moves->moves[i]);
        if (h_scores[i] > max_h) {
            max_h = h_scores[i];
        }
    }

    float sum_exp = 0.0f;
    for (int i = 0; i < moves->count; i++) {
        float ex = expf((h_scores[i] - max_h) / tau);
        priors[i] = ex;
        sum_exp += ex;
    }

    float inv_sum = (sum_exp > 0.0f) ? (1.0f / sum_exp) : (1.0f / (float)moves->count);
    for (int i = 0; i < moves->count; i++) {
        priors[i] *= inv_sum;
    }
}

void engine_mcts_puct_init(void **state) {
    wld_db_init();
    PUCTEngineState *st = (PUCTEngineState*)malloc(sizeof(PUCTEngineState));
    if (!st) {
        *state = NULL;
        return;
    }
    memset(st, 0, sizeof(PUCTEngineState));
    st->time_budget = PUCT_DEFAULT_TIME_BUDGET;
    st->c_puct = PUCT_DEFAULT_C_PUCT;
    st->temperature = PUCT_DEFAULT_TEMPERATURE;
    st->max_rollout_depth = PUCT_MAX_ROLLOUT_DEPTH;
    st->use_db = true;
    st->debug_log = false;
    st->root_idx = UINT32_MAX;
    st->rng_state = (uint32_t)time(NULL) ^ 0xA55AA55AU;
    st->has_prev_state = false;
    st->prev_ai_move = MOVE_NONE;
    *state = st;
}

void engine_mcts_puct_cleanup(void *state) {
    if (state) {
        free(state);
    }
}

void engine_mcts_puct_set_time_budget(void *state, double seconds) {
    if (state && seconds > 0.0) {
        ((PUCTEngineState*)state)->time_budget = seconds;
    }
}

void engine_mcts_puct_set_c_puct(void *state, float c_puct) {
    if (state && c_puct >= 0.0f) {
        ((PUCTEngineState*)state)->c_puct = c_puct;
    }
}

void engine_mcts_puct_set_temperature(void *state, float tau) {
    if (state && tau > 0.0f) {
        ((PUCTEngineState*)state)->temperature = tau;
    }
}

void engine_mcts_puct_set_max_rollout_depth(void *state, int depth) {
    if (state && depth > 0) {
        ((PUCTEngineState*)state)->max_rollout_depth = depth;
    }
}

void engine_mcts_puct_set_use_db(void *state, bool enable) {
    if (state) {
        ((PUCTEngineState*)state)->use_db = enable;
    }
}

void engine_mcts_puct_set_debug_log(void *state, bool enable) {
    if (state) {
        ((PUCTEngineState*)state)->debug_log = enable;
    }
}

uint32_t engine_mcts_puct_get_node_count(void) {
    return s_pool_tail;
}

uint32_t engine_mcts_puct_get_root_visits(void *state) {
    if (!state) return 0;
    PUCTEngineState *st = (PUCTEngineState*)state;
    if (st->root_idx == UINT32_MAX || st->root_idx >= s_pool_tail) return 0;
    return s_node_pool[st->root_idx].visits;
}

static bool boards_equal(const Board *a, const Board *b) {
    return (a->white_men == b->white_men) &&
           (a->white_kings == b->white_kings) &&
           (a->black_men == b->black_men) &&
           (a->black_kings == b->black_kings);
}

// Rollout evaluation cutoff with WLD tablebase integration
static float evaluate_rollout_terminal(const GameState *sim_state, Player ai_player) {
    if (sim_state->is_game_over) {
        if (sim_state->is_draw) {
            return 0.5f; // Draw
        }
        if (sim_state->winner == ai_player) {
            return 1.0f; // Win
        } else {
            return 0.0f; // Loss
        }
    }

    // Exact Endgame Tablebase Probe (<= 5 pieces)
    if (wld_db_is_endgame(&sim_state->board)) {
        WLDValue wld = wld_db_probe(sim_state);
        if (wld == WLD_WIN_WHITE) {
            return (ai_player == PLAYER_WHITE) ? 1.0f : 0.0f;
        } else if (wld == WLD_WIN_BLACK) {
            return (ai_player == PLAYER_BLACK) ? 1.0f : 0.0f;
        } else if (wld == WLD_DRAW) {
            return 0.5f;
        }
    }

    // Heuristic material evaluation at cutoff
    int w_men = __builtin_popcount(sim_state->board.white_men);
    int w_kings = __builtin_popcount(sim_state->board.white_kings);
    int b_men = __builtin_popcount(sim_state->board.black_men);
    int b_kings = __builtin_popcount(sim_state->board.black_kings);

    int w_score = w_men + 3 * w_kings;
    int b_score = b_men + 3 * b_kings;

    if (w_score == b_score) {
        return 0.5f; // Draw
    }

    if (ai_player == PLAYER_WHITE) {
        return (w_score > b_score) ? 1.0f : 0.0f;
    } else {
        return (b_score > w_score) ? 1.0f : 0.0f;
    }
}

static inline int sq_chebyshev_dist(int sq1, int sq2) {
    int r1 = SQ_TO_ROW(sq1), c1 = SQ_TO_COL(sq1);
    int r2 = SQ_TO_ROW(sq2), c2 = SQ_TO_COL(sq2);
    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);
    return dr > dc ? dr : dc;
}

static inline int sq_manhattan_dist(int sq1, int sq2) {
    int r1 = SQ_TO_ROW(sq1), c1 = SQ_TO_COL(sq1);
    int r2 = SQ_TO_ROW(sq2), c2 = SQ_TO_COL(sq2);
    return abs(r1 - r2) + abs(c1 - c2);
}

static int calculate_enemy_pursuit_distance(int to_sq, const Board *board, Player ai_player) {
    uint32_t enemy_kings = (ai_player == PLAYER_WHITE) ? board->black_kings : board->white_kings;
    uint32_t enemy_men   = (ai_player == PLAYER_WHITE) ? board->black_men : board->white_men;

    if (enemy_kings != 0) {
        int min_d = 999;
        uint32_t mask = enemy_kings;
        while (mask) {
            int sq = __builtin_ctz(mask);
            mask &= mask - 1;
            int d = sq_chebyshev_dist(to_sq, sq) * 10 + sq_manhattan_dist(to_sq, sq);
            if (d < min_d) min_d = d;
        }
        return min_d;
    }

    if (enemy_men != 0) {
        int min_d = 999;
        uint32_t mask = enemy_men;
        while (mask) {
            int sq = __builtin_ctz(mask);
            mask &= mask - 1;
            int d = sq_chebyshev_dist(to_sq, sq) * 10 + sq_manhattan_dist(to_sq, sq);
            if (d < min_d) min_d = d;
        }
        return min_d;
    }

    return 0;
}

typedef enum {
    MOVE_EVAL_LOSS = 0,
    MOVE_EVAL_DRAW = 1,
    MOVE_EVAL_WIN  = 2
} MoveOutcome;

// Direct Endgame Database Move Selector (Bypasses rollouts in endgame)
static Move puct_select_database_move(const GameState *game, const MoveList *valid_moves, bool debug_log) {
    Player ai_player = game->current_player;

    Move best_win_move = MOVE_NONE;
    int best_win_score = 99999;

    Move best_draw_move = MOVE_NONE;
    int best_draw_score = 99999;

    Move best_loss_move = MOVE_NONE;
    int best_loss_score = -99999;

    if (debug_log) {
        int w_cnt = __builtin_popcount(game->board.white_men) + __builtin_popcount(game->board.white_kings);
        int b_cnt = __builtin_popcount(game->board.black_men) + __builtin_popcount(game->board.black_kings);
        printf("\n================================================================================\n");
        printf("[MCTS PUCT DEBUG LOG - DATABASE MODE]\n");
        printf("Giocatore: %s | Pezzi in gioco: %d Bianco, %d Nero | Mosse candidate: %d\n",
               (ai_player == PLAYER_WHITE) ? "BIANCO" : "NERO", w_cnt, b_cnt, valid_moves->count);
        printf("Valutazione Tablebase WLD esatta con Minimax a 1-ply sulle risposte avversarie:\n");
        printf("--------------------------------------------------------------------------------\n");
        printf("  # | Mossa            | Esito WLD  | Dist. Target | Ripetizioni | Punteggio\n");
        printf("--------------------------------------------------------------------------------\n");
    }

    for (int i = 0; i < valid_moves->count; i++) {
        Move m = valid_moves->moves[i];
        GameState test_state = *game;
        game_execute_move(&test_state, m);

        MoveOutcome outcome = MOVE_EVAL_LOSS;

        if (test_state.is_game_over) {
            if (test_state.is_draw) {
                outcome = MOVE_EVAL_DRAW;
            } else if (test_state.winner == ai_player) {
                outcome = MOVE_EVAL_WIN;
            } else {
                outcome = MOVE_EVAL_LOSS;
            }
        } else {
            MoveList opp_moves = *game_get_valid_moves(&test_state);
            if (opp_moves.count == 0) {
                outcome = MOVE_EVAL_WIN;
            } else {
                MoveOutcome worst_for_ai = MOVE_EVAL_WIN;
                for (int j = 0; j < opp_moves.count; j++) {
                    GameState opp_state = test_state;
                    game_execute_move(&opp_state, opp_moves.moves[j]);

                    MoveOutcome res = MOVE_EVAL_LOSS;
                    if (opp_state.is_game_over) {
                        if (opp_state.is_draw) {
                            res = MOVE_EVAL_DRAW;
                        } else if (opp_state.winner == ai_player) {
                            res = MOVE_EVAL_WIN;
                        } else {
                            res = MOVE_EVAL_LOSS;
                        }
                    } else {
                        WLDValue wld = wld_db_probe(&opp_state);
                        if (wld == WLD_WIN_WHITE) {
                            res = (ai_player == PLAYER_WHITE) ? MOVE_EVAL_WIN : MOVE_EVAL_LOSS;
                        } else if (wld == WLD_WIN_BLACK) {
                            res = (ai_player == PLAYER_BLACK) ? MOVE_EVAL_WIN : MOVE_EVAL_LOSS;
                        } else if (wld == WLD_DRAW) {
                            res = MOVE_EVAL_DRAW;
                        } else {
                            res = MOVE_EVAL_DRAW;
                        }
                    }

                    if (res < worst_for_ai) {
                        worst_for_ai = res;
                    }
                    if (worst_for_ai == MOVE_EVAL_LOSS) {
                        break;
                    }
                }
                outcome = worst_for_ai;
            }
        }

        int rep = game_get_repetition_count(&test_state);
        if (rep >= 3 && outcome == MOVE_EVAL_WIN) {
            outcome = MOVE_EVAL_DRAW;
        }

        int to_sq = MOVE_TO(m);
        int dist = calculate_enemy_pursuit_distance(to_sq, &test_state.board, ai_player);

        if (MOVE_IS_CAP(m)) {
            dist -= 50;
        }
        if (MOVE_IS_PROM(m)) {
            dist -= 30;
        }

        int rep_penalty = (rep >= 2) ? 200 : 0;
        int win_score = dist + rep_penalty;
        int draw_score = dist + (rep >= 3 ? 0 : 50);
        int loss_score = dist;

        if (debug_log) {
            const char *out_str = (outcome == MOVE_EVAL_WIN) ? "WIN " : ((outcome == MOVE_EVAL_DRAW) ? "DRAW" : "LOSS");
            int final_score = (outcome == MOVE_EVAL_WIN) ? win_score : ((outcome == MOVE_EVAL_DRAW) ? draw_score : loss_score);
            printf(" %2d | %02d(r%d,c%d)->%02d(r%d,c%d) | %s       | dist: %3d      | rep: %d      | score: %5d\n",
                   i + 1,
                   MOVE_FROM(m), SQ_TO_ROW(MOVE_FROM(m)), SQ_TO_COL(MOVE_FROM(m)),
                   MOVE_TO(m), SQ_TO_ROW(MOVE_TO(m)), SQ_TO_COL(MOVE_TO(m)),
                   out_str, dist, rep, final_score);
        }

        if (outcome == MOVE_EVAL_WIN) {
            if (move_is_none(best_win_move) || win_score < best_win_score) {
                best_win_score = win_score;
                best_win_move = m;
            }
        } else if (outcome == MOVE_EVAL_DRAW) {
            if (move_is_none(best_draw_move) || draw_score < best_draw_score) {
                best_draw_score = draw_score;
                best_draw_move = m;
            }
        } else {
            if (move_is_none(best_loss_move) || loss_score > best_loss_score) {
                best_loss_score = loss_score;
                best_loss_move = m;
            }
        }
    }

    Move chosen = valid_moves->moves[0];
    if (!move_is_none(best_win_move)) {
        chosen = best_win_move;
    } else if (!move_is_none(best_draw_move)) {
        chosen = best_draw_move;
    } else if (!move_is_none(best_loss_move)) {
        chosen = best_loss_move;
    }

    if (debug_log) {
        printf("--------------------------------------------------------------------------------\n");
        printf("Mossa selezionata da Database: %02d(r%d,c%d) -> %02d(r%d,c%d)\n",
               MOVE_FROM(chosen), SQ_TO_ROW(MOVE_FROM(chosen)), SQ_TO_COL(MOVE_FROM(chosen)),
               MOVE_TO(chosen), SQ_TO_ROW(MOVE_TO(chosen)), SQ_TO_COL(MOVE_TO(chosen)));
        printf("================================================================================\n\n");
        fflush(stdout);
    }

    return chosen;
}

// Select best child using PUCT formula:
// PUCT(s, a) = Q(s, a) + c_puct * P(s, a) * [ sqrt(sum_b N(s, b)) / (1 + N(s, a)) ]
static uint32_t puct_select_child(uint32_t parent_idx, float c_puct) {
    uint32_t fc = s_node_pool[parent_idx].first_child_idx;
    uint8_t nc = s_node_pool[parent_idx].num_children;
    if (fc == UINT32_MAX || nc == 0) return UINT32_MAX;

    uint32_t sum_n = 0;
    for (uint8_t i = 0; i < nc; i++) {
        sum_n += s_node_pool[fc + i].visits;
    }

    float sqrt_sum = (sum_n > 0) ? sqrtf((float)sum_n) : 1.0f;

    uint32_t best_child = fc;
    float best_val = -1e9f;

    for (uint8_t i = 0; i < nc; i++) {
        uint32_t c_idx = fc + i;
        uint32_t n = s_node_pool[c_idx].visits;
        float q = (n > 0) ? (s_node_pool[c_idx].wins / (float)n) : 0.0f;
        float p = s_node_pool[c_idx].prior;
        float u = c_puct * p * (sqrt_sum / (1.0f + (float)n));
        float val = q + u;

        if (val > best_val) {
            best_val = val;
            best_child = c_idx;
        }
    }

    return best_child;
}

// Expand node in PUCT tree and assign domain heuristic priors
static bool puct_expand_node(uint32_t node_idx, const GameState *state, float temperature) {
    if (s_node_pool[node_idx].first_child_idx != UINT32_MAX) return true;

    MoveList ml = *game_get_valid_moves(state);
    s_node_pool[node_idx].num_children = ml.count;
    s_node_pool[node_idx].unexpanded_idx = 0;

    if (ml.count == 0) {
        return false;
    }

    if (s_pool_tail + ml.count > PUCT_MAX_NODES) {
        return false;
    }

    float priors[48];
    compute_priors(state, &ml, temperature, priors);

    uint32_t start_c = s_pool_tail;
    s_pool_tail += ml.count;
    s_node_pool[node_idx].first_child_idx = start_c;

    for (uint8_t i = 0; i < ml.count; i++) {
        s_node_pool[start_c + i].visits = 0;
        s_node_pool[start_c + i].wins = 0.0f;
        s_node_pool[start_c + i].prior = priors[i];
        s_node_pool[start_c + i].parent_idx = node_idx;
        s_node_pool[start_c + i].first_child_idx = UINT32_MAX;
        s_node_pool[start_c + i].move = ml.moves[i];
        s_node_pool[start_c + i].num_children = 0;
        s_node_pool[start_c + i].unexpanded_idx = 0;
    }

    return true;
}

Move engine_mcts_puct_get_move(void *state, const GameState *game) {
    if (!state || !game || game->is_game_over) {
        return MOVE_NONE;
    }

    PUCTEngineState *st = (PUCTEngineState*)state;
    Player ai_player = game->current_player;

    // Check valid moves at current state
    MoveList root_valid_moves = *game_get_valid_moves(game);
    if (root_valid_moves.count == 0) {
        return MOVE_NONE;
    }
    if (root_valid_moves.count == 1) {
        if (st->debug_log) {
            Move fm = root_valid_moves.moves[0];
            printf("\n[MCTS PUCT DEBUG] Giocatore: %s | Mossa unica forzata: %02d(r%d,c%d) -> %02d(r%d,c%d)\n\n",
                   (ai_player == PLAYER_WHITE) ? "BIANCO" : "NERO",
                   MOVE_FROM(fm), SQ_TO_ROW(MOVE_FROM(fm)), SQ_TO_COL(MOVE_FROM(fm)),
                   MOVE_TO(fm), SQ_TO_ROW(MOVE_TO(fm)), SQ_TO_COL(MOVE_TO(fm)));
            fflush(stdout);
        }
        st->has_prev_state = true;
        st->prev_game_state = *game;
        st->prev_ai_move = root_valid_moves.moves[0];
        return root_valid_moves.moves[0];
    }

    // Direct Database Mode: If enabled and <= 5 pieces remain
    if (st->use_db && wld_db_is_endgame(&game->board)) {
        Move db_move = puct_select_database_move(game, &root_valid_moves, st->debug_log);
        st->has_prev_state = true;
        st->prev_game_state = *game;
        st->prev_ai_move = db_move;
        return db_move;
    }

    // Safety threshold check: if pool is 80%+ full, reset to prevent overflow
    if (s_pool_tail >= PUCT_SAFETY_THRESHOLD_NODES) {
        pool_reset();
        st->root_idx = UINT32_MAX;
    }

    // Subtree Promotion (Tree Reuse)
    bool tree_reused = false;
    if (st->has_prev_state && st->root_idx != UINT32_MAX && st->root_idx < s_pool_tail) {
        uint32_t ai_child_idx = UINT32_MAX;
        if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX) {
            uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
            uint8_t nc = s_node_pool[st->root_idx].num_children;
            for (uint8_t i = 0; i < nc; i++) {
                if (move_equals(s_node_pool[fc + i].move, st->prev_ai_move)) {
                    ai_child_idx = fc + i;
                    break;
                }
            }
        }

        if (ai_child_idx != UINT32_MAX && s_node_pool[ai_child_idx].first_child_idx != UINT32_MAX) {
            GameState after_ai = st->prev_game_state;
            game_execute_move(&after_ai, st->prev_ai_move);

            uint32_t opp_fc = s_node_pool[ai_child_idx].first_child_idx;
            uint8_t opp_nc = s_node_pool[ai_child_idx].num_children;
            for (uint8_t j = 0; j < opp_nc; j++) {
                GameState test_state = after_ai;
                game_execute_move(&test_state, s_node_pool[opp_fc + j].move);
                if (boards_equal(&test_state.board, &game->board)) {
                    st->root_idx = opp_fc + j;
                    s_node_pool[st->root_idx].parent_idx = UINT32_MAX;
                    tree_reused = true;
                    break;
                }
            }
        }
    }

    if (!tree_reused) {
        pool_reset();
        st->root_idx = create_root_node();
    }

    // Ensure root children are generated with domain heuristic priors
    if (s_node_pool[st->root_idx].first_child_idx == UINT32_MAX) {
        puct_expand_node(st->root_idx, game, st->temperature);
    }

    // High-performance Anytime Search Loop with PUCT Policy
    double start_time = puct_get_time();
    uint32_t iterations = 0;
    uint32_t path_stack[MAX_TREE_DEPTH];

    const float c_puct = st->c_puct;
    const float temperature = st->temperature;
    const double budget = st->time_budget;

    while (1) {
        if ((iterations & 1023) == 0 && iterations > 0) {
            double elapsed = puct_get_time() - start_time;
            if (elapsed >= budget) {
                break;
            }
            if (s_pool_tail + 64 >= PUCT_MAX_NODES) {
                break;
            }
        }

        // 1. SELECTION
        int path_len = 0;
        uint32_t curr_idx = st->root_idx;
        GameState curr_state = *game;
        path_stack[path_len++] = curr_idx;

        while (s_node_pool[curr_idx].first_child_idx != UINT32_MAX && !curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            uint32_t best_child = puct_select_child(curr_idx, c_puct);
            if (best_child == UINT32_MAX) break;

            game_execute_move(&curr_state, s_node_pool[best_child].move);
            path_stack[path_len++] = best_child;
            curr_idx = best_child;

            // If an unvisited child is reached, stop selection and proceed to simulation
            if (s_node_pool[curr_idx].visits == 0) {
                break;
            }
        }

        // 2. EXPANSION (if reached node was previously visited and not expanded yet)
        if (!curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            if (s_node_pool[curr_idx].first_child_idx == UINT32_MAX) {
                if (puct_expand_node(curr_idx, &curr_state, temperature)) {
                    // Pick top child by PUCT/prior to step into
                    uint32_t best_child = puct_select_child(curr_idx, c_puct);
                    if (best_child != UINT32_MAX) {
                        game_execute_move(&curr_state, s_node_pool[best_child].move);
                        path_stack[path_len++] = best_child;
                        curr_idx = best_child;
                    }
                }
            }
        }

        // 3. SIMULATION (ROLLOUT)
        GameState rollout_state = curr_state;
        int rollout_depth = 0;
        int max_depth = st->max_rollout_depth;
        while (!rollout_state.is_game_over && rollout_depth < max_depth) {
            if (st->use_db && wld_db_is_endgame(&rollout_state.board)) {
                WLDValue wld = wld_db_probe(&rollout_state);
                if (wld != WLD_UNKNOWN) {
                    break;
                }
            }

            MoveList ml = *game_get_valid_moves(&rollout_state);
            if (ml.count == 0) {
                rollout_state.is_game_over = true;
                rollout_state.winner = (rollout_state.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                break;
            }
            uint32_t r = xorshift32(&st->rng_state);
            Move rm = ml.moves[r % ml.count];
            game_execute_move(&rollout_state, rm);
            rollout_depth++;
        }

        float ai_reward = evaluate_rollout_terminal(&rollout_state, ai_player);

        // 4. BACKPROPAGATION
        for (int p = 0; p < path_len; p++) {
            uint32_t n_idx = path_stack[p];
            s_node_pool[n_idx].visits += 1;

            if (p == 0) {
                s_node_pool[n_idx].wins += ai_reward;
            } else if (p % 2 == 1) {
                s_node_pool[n_idx].wins += ai_reward;
            } else {
                s_node_pool[n_idx].wins += (1.0f - ai_reward);
            }
        }

        iterations++;
    }

    // ROBUST CHILD SELECTION (child with max visits)
    uint32_t best_child = UINT32_MAX;
    uint32_t max_visits = 0;
    float max_q = -1.0f;

    if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX) {
        uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
        uint8_t nc = s_node_pool[st->root_idx].num_children;
        for (uint8_t i = 0; i < nc; i++) {
            uint32_t c_idx = fc + i;
            uint32_t v = s_node_pool[c_idx].visits;
            float q = (v > 0) ? (s_node_pool[c_idx].wins / (float)v) : 0.0f;
            if (v > max_visits || (v == max_visits && q > max_q)) {
                max_visits = v;
                max_q = q;
                best_child = c_idx;
            }
        }
    }

    Move selected_move = (best_child != UINT32_MAX) ? s_node_pool[best_child].move : root_valid_moves.moves[0];

    if (st->debug_log) {
        double elapsed = puct_get_time() - start_time;
        printf("\n================================================================================\n");
        printf("[MCTS PUCT DEBUG LOG - PREDICTOR UPPER CONFIDENCE TREE]\n");
        printf("Giocatore: %s | Mosse legali: %d | Nodi pool allocati: %u / %d\n",
               (ai_player == PLAYER_WHITE) ? "BIANCO" : "NERO", root_valid_moves.count, s_pool_tail, PUCT_MAX_NODES);
        printf("Budget tempo: %.2fs | Tempo impiegato: %.3fs | Simulazioni totali: %u\n",
               st->time_budget, elapsed, iterations);
        printf("Parametri: c_puct = %.2f | Temp (tau) = %.2f | Profondita Max Rollout = %d\n",
               st->c_puct, st->temperature, st->max_rollout_depth);
        printf("--------------------------------------------------------------------------------\n");
        printf("  # | Mossa            | Prior P(s,a) | Visite (N)     | Win Rate (w/N) | Q      | Note\n");
        printf("--------------------------------------------------------------------------------\n");

        if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX) {
            uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
            uint8_t nc = s_node_pool[st->root_idx].num_children;
            uint32_t total_visits = s_node_pool[st->root_idx].visits;
            for (uint8_t i = 0; i < nc; i++) {
                uint32_t c_idx = fc + i;
                Move cm = s_node_pool[c_idx].move;
                uint32_t v = s_node_pool[c_idx].visits;
                float p = s_node_pool[c_idx].prior;
                float q = (v > 0) ? (s_node_pool[c_idx].wins / (float)v) : 0.0f;
                float pct = (total_visits > 0) ? ((float)v * 100.0f / (float)total_visits) : 0.0f;
                const char *note = (c_idx == best_child) ? "[*SCELTA*]" : "";

                printf(" %2d | %02d(r%d,c%d)->%02d(r%d,c%d) | %10.4f   | %7u (%5.1f%%) | %6.1f%%        | %6.4f | %s\n",
                       i + 1,
                       MOVE_FROM(cm), SQ_TO_ROW(MOVE_FROM(cm)), SQ_TO_COL(MOVE_FROM(cm)),
                       MOVE_TO(cm), SQ_TO_ROW(MOVE_TO(cm)), SQ_TO_COL(MOVE_TO(cm)),
                       p, v, pct, q * 100.0f, q, note);
            }
        }
        printf("--------------------------------------------------------------------------------\n");
        printf("Mossa scelta: %02d(r%d,c%d) -> %02d(r%d,c%d) (Visite: %u, WinRate: %.1f%%)\n",
               MOVE_FROM(selected_move), SQ_TO_ROW(MOVE_FROM(selected_move)), SQ_TO_COL(MOVE_FROM(selected_move)),
               MOVE_TO(selected_move), SQ_TO_ROW(MOVE_TO(selected_move)), SQ_TO_COL(MOVE_TO(selected_move)),
               max_visits, max_q * 100.0f);
        printf("================================================================================\n\n");
        fflush(stdout);
    }

    // Record for future subtree promotion
    st->has_prev_state = true;
    st->prev_game_state = *game;
    st->prev_ai_move = selected_move;

    return selected_move;
}
