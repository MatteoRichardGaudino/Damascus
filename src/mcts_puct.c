/*______________________________________________________________________________
  MCTS_PUCT Italian Checkers Engine (Dama Italiana)
  High-Performance Monte Carlo Tree Search with PUCT & Prior Domain Heuristics
  Zero dynamic allocations during search, Anytime budget compliance, Subtree reuse
______________________________________________________________________________*/

#include "mcts_puct.h"
#include "mcts_heuristic.h"
#include "opening_book.h"
#include "wld_db.h"
#include "zobrist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_TREE_DEPTH 256

typedef struct {
    double             time_budget;
    float              c_puct;
    float              temperature;
    int                max_rollout_depth;
    float              rollout_epsilon;
    bool               use_db;
    bool               use_book;
    BookPlayMode       book_mode;
    float              book_temperature;
    bool               debug_log;
    uint32_t           root_idx;
    uint32_t           rng_state;
    bool               has_prev_state;
    GameState          prev_game_state;
    Move               prev_ai_move;
    TranspositionTable tt;
    uint16_t           search_epoch;
    EngineStats        last_stats;
} PUCTEngineState;

// Per-thread dynamic preallocated memory pool of 2,000,000 nodes (~48 MB per active thread)
static _Thread_local PUCTNode *s_node_pool = NULL;
static _Thread_local uint32_t s_pool_tail = 0;
static _Thread_local void *s_pool_owner = NULL;

#ifdef _WIN32
#include <windows.h>
static inline double puct_get_time(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
static inline double puct_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0x85431249U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void pool_ensure(void) {
    if (!s_node_pool) {
        s_node_pool = (PUCTNode*)calloc(PUCT_MAX_NODES, sizeof(PUCTNode));
        s_pool_tail = 0;
        s_pool_owner = NULL;
    }
}

static void pool_reset(void) {
    s_pool_tail = 0;
}

static uint32_t create_root_node(uint64_t hash) {
    pool_ensure();
    if (s_pool_tail >= PUCT_MAX_NODES) {
        pool_reset();
    }
    uint32_t idx = s_pool_tail++;
    s_node_pool[idx].hash = hash;
    s_node_pool[idx].visits = 0;
    s_node_pool[idx].wins = 0.0f;
    s_node_pool[idx].prior = 1.0f;
    s_node_pool[idx].parent_idx = UINT32_MAX;
    s_node_pool[idx].first_child_idx = UINT32_MAX;
    s_node_pool[idx].move = MOVE_NONE;
    s_node_pool[idx].num_children = 0;
    s_node_pool[idx].unexpanded_idx = 0;
    s_node_pool[idx].proof_status = MCTS_PROOF_UNKNOWN;
    s_node_pool[idx].proof_depth = 0;
    return idx;
}

/* Fast Domain Heuristic H(s, a) for Italian Draughts (FID rules) */
float puct_compute_heuristic(const GameState *state, Move move) {
    return mcts_compute_move_heuristic(state, move);
}

float puct_compute_heuristic_compact(const CompactState *state, Move move) {
    return mcts_compute_move_heuristic_compact(state, move);
}

// Compute Softmax Prior Policy P(s, a) = exp(H(s, a) / tau) / sum(exp(H(s, b) / tau))
// Blended with opening book distribution P_book(s, a) when use_book is active
static void compute_priors(const CompactState *state, const MoveList *moves, float tau, bool use_book, float *priors) {
    if (moves->count == 0) return;
    if (moves->count == 1) {
        priors[0] = 1.0f;
        return;
    }

    if (tau <= 0.001f) tau = 1.0f;

    float h_scores[48];
    float max_h = -1e9f;

    for (int i = 0; i < moves->count; i++) {
        h_scores[i] = mcts_compute_move_heuristic_compact(state, moves->moves[i]);
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

    // Blend opening book distribution P_book(s, a) if book is enabled and state has entries
    if (use_book) {
        BookMoveList book_moves;
        if (opening_book_probe_compact(state, &book_moves) && book_moves.count > 0) {
            const float lambda_book = 0.75f; // AlphaGo style blending parameter
            float blended[48];
            float sum_blended = 0.0f;

            for (int i = 0; i < moves->count; i++) {
                Move m = moves->moves[i];
                float p_book = 0.0f;
                for (int b = 0; b < book_moves.count; b++) {
                    if (move_equals(m, book_moves.entries[b].move)) {
                        p_book = book_moves.entries[b].prior_weight;
                        break;
                    }
                }
                blended[i] = (1.0f - lambda_book) * priors[i] + lambda_book * p_book;
                sum_blended += blended[i];
            }

            if (sum_blended > 0.0f) {
                float inv_blended = 1.0f / sum_blended;
                for (int i = 0; i < moves->count; i++) {
                    priors[i] = blended[i] * inv_blended;
                }
            }
        }
    }
}

void engine_mcts_puct_init(void **state) {
    wld_db_init();
    opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    zobrist_init();
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
    st->rollout_epsilon = PUCT_DEFAULT_ROLLOUT_EPSILON;
    st->use_db = false;
    st->use_book = false;
    st->book_mode = BOOK_MODE_GOOD;
    st->book_temperature = 2.4828f;
    st->debug_log = false;
    st->root_idx = UINT32_MAX;
    st->rng_state = (uint32_t)time(NULL) ^ 0xA55AA55AU;
    st->has_prev_state = false;
    st->prev_ai_move = MOVE_NONE;
    st->search_epoch = 1;
    tt_init(&st->tt, TT_DEFAULT_SIZE);
    *state = st;
}

void engine_mcts_puct_reset(void *state) {
    if (!state) return;
    PUCTEngineState *st = (PUCTEngineState*)state;
    st->has_prev_state = false;
    st->root_idx = UINT32_MAX;
    st->prev_ai_move = MOVE_NONE;
    tt_clear(&st->tt);
    st->search_epoch = 1;
}

void engine_mcts_puct_cleanup(void *state) {
    if (state) {
        PUCTEngineState *st = (PUCTEngineState*)state;
        tt_free(&st->tt);
        free(st);
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

void engine_mcts_puct_set_rollout_epsilon(void *state, float epsilon) {
    if (state && epsilon >= 0.0f && epsilon <= 1.0f) {
        ((PUCTEngineState*)state)->rollout_epsilon = epsilon;
    }
}

void engine_mcts_puct_set_use_db(void *state, bool enable) {
    if (state) {
        ((PUCTEngineState*)state)->use_db = enable;
    }
}

void engine_mcts_puct_set_use_book(void *state, bool enable) {
    if (state) {
        ((PUCTEngineState*)state)->use_book = enable;
    }
}

void engine_mcts_puct_set_book_mode(void *state, BookPlayMode mode) {
    if (state) {
        ((PUCTEngineState*)state)->book_mode = mode;
    }
}

void engine_mcts_puct_set_book_temperature(void *state, float tau) {
    if (state && tau > 0.0f) {
        ((PUCTEngineState*)state)->book_temperature = tau;
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
    if (!state || !s_node_pool) return 0;
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

/// Rollout evaluation cutoff with heuristic material evaluation and depth discounting
static float evaluate_rollout_terminal(const CompactState *sim_state, Player ai_player, int total_depth) {
    if (sim_state->is_game_over) {
        return mcts_compute_depth_discounted_reward(
            !sim_state->is_draw && (sim_state->winner == ai_player),
            !sim_state->is_draw && (sim_state->winner != ai_player),
            sim_state->is_draw,
            total_depth
        );
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

    bool ai_winning = (ai_player == PLAYER_WHITE) ? (w_score > b_score) : (b_score > w_score);
    return mcts_compute_depth_discounted_reward(ai_winning, !ai_winning, false, total_depth);
}

// Initialize node game-theoretic proof status from terminal state or tablebase probe
static inline void mcts_init_puct_node_proof(PUCTNode *node, const CompactState *parent_state, const CompactState *child_state, bool use_db) {
    node->proof_status = MCTS_PROOF_UNKNOWN;
    node->proof_depth = 0;

    if (child_state->is_game_over) {
        node->visits = 1;
        if (child_state->is_draw) {
            node->wins = 0.5f;
            node->proof_status = MCTS_PROOF_DRAW;
            node->proof_depth = 0;
        } else if (child_state->winner == parent_state->current_player) {
            node->wins = 1.0f;
            node->proof_status = MCTS_PROOF_LOSS; // Proven loss for opponent who is next to move at child_state
            node->proof_depth = 0;
        } else {
            node->wins = 0.0f;
            node->proof_status = MCTS_PROOF_WIN; // Proven win for opponent who is next to move at child_state
            node->proof_depth = 0;
        }
        return;
    }

    if (use_db && (wld_is_endgame_compact(child_state) || wld_db_is_endgame(&child_state->board))) {
        WLDValue wld = wld_probe_compact(child_state);
        if (wld == WLD_WIN_WHITE) {
            node->visits = 1;
            node->wins = (parent_state->current_player == PLAYER_WHITE) ? 1.0f : 0.0f;
        } else if (wld == WLD_WIN_BLACK) {
            node->visits = 1;
            node->wins = (parent_state->current_player == PLAYER_BLACK) ? 1.0f : 0.0f;
        } else if (wld == WLD_DRAW) {
            node->visits = 1;
            node->wins = 0.5f;
        }
    }
}

// Update proof status of node based on child proof states (MCTS-Solver propagation)
static inline void mcts_update_puct_proof_status(uint32_t node_idx) {
    if (s_node_pool[node_idx].first_child_idx == UINT32_MAX) return;
    uint8_t nc = s_node_pool[node_idx].num_children;
    if (nc == 0) {
        s_node_pool[node_idx].proof_status = MCTS_PROOF_LOSS;
        s_node_pool[node_idx].proof_depth = 0;
        return;
    }

    uint32_t fc = s_node_pool[node_idx].first_child_idx;
    bool has_loss_child = false;
    uint8_t min_loss_depth = 255;
    bool all_win_children = true;
    uint8_t max_win_depth = 0;
    bool all_proven = true;
    uint8_t min_draw_depth = 255;

    for (uint8_t i = 0; i < nc; i++) {
        uint32_t c_idx = fc + i;
        uint8_t st = s_node_pool[c_idx].proof_status;
        uint8_t d = s_node_pool[c_idx].proof_depth;

        if (st == MCTS_PROOF_LOSS) {
            has_loss_child = true;
            if (d < min_loss_depth) min_loss_depth = d;
        }
        if (st != MCTS_PROOF_WIN) {
            all_win_children = false;
        } else {
            if (d > max_win_depth) max_win_depth = d;
        }
        if (st == MCTS_PROOF_UNKNOWN) {
            all_proven = false;
        }
        if (st == MCTS_PROOF_DRAW) {
            if (d < min_draw_depth) min_draw_depth = d;
        }
    }

    if (has_loss_child) {
        s_node_pool[node_idx].proof_status = MCTS_PROOF_WIN;
        s_node_pool[node_idx].proof_depth = (min_loss_depth < 254) ? (min_loss_depth + 1) : 255;
    } else if (all_win_children) {
        s_node_pool[node_idx].proof_status = MCTS_PROOF_LOSS;
        s_node_pool[node_idx].proof_depth = (max_win_depth < 254) ? (max_win_depth + 1) : 255;
    } else if (all_proven) {
        s_node_pool[node_idx].proof_status = MCTS_PROOF_DRAW;
        s_node_pool[node_idx].proof_depth = (min_draw_depth < 254) ? (min_draw_depth + 1) : 255;
    }
}


// Select best child using AlphaZero PUCT formula:
// UCB(s, a) = Q(s, a) + c_puct * P(s, a) * (sqrt(sum(N(s, b))) / (1 + N(s, a)))
// Proof status overrides: Proven win moves have top priority, proven losses heavily penalized.
static inline uint32_t puct_select_child(uint32_t node_idx, float c_puct) {
    if (s_node_pool[node_idx].first_child_idx == UINT32_MAX) return UINT32_MAX;

    uint32_t fc = s_node_pool[node_idx].first_child_idx;
    uint8_t nc = s_node_pool[node_idx].num_children;
    if (nc == 0) return UINT32_MAX;

    // 1. Solver Proof Status Shortcuts: If there is a proven win (child proof_status == MCTS_PROOF_LOSS), select it
    uint8_t min_win_depth = 255;
    uint32_t best_win_child = UINT32_MAX;
    for (uint8_t i = 0; i < nc; i++) {
        uint32_t c_idx = fc + i;
        if (s_node_pool[c_idx].proof_status == MCTS_PROOF_LOSS) {
            if (s_node_pool[c_idx].proof_depth < min_win_depth) {
                min_win_depth = s_node_pool[c_idx].proof_depth;
                best_win_child = c_idx;
            }
        }
    }
    if (best_win_child != UINT32_MAX) {
        return best_win_child;
    }

    // 2. Standard PUCT Selection
    uint32_t sum_n = 0;
    for (uint8_t i = 0; i < nc; i++) {
        sum_n += s_node_pool[fc + i].visits;
    }
    float sqrt_sum = sqrtf((float)sum_n);

    uint32_t best_child = fc;
    float best_val = -1e9f;

    for (uint8_t i = 0; i < nc; i++) {
        uint32_t c_idx = fc + i;
        uint8_t c_proof = s_node_pool[c_idx].proof_status;
        float val;

        if (c_proof == MCTS_PROOF_LOSS) {
            val = 10000.0f - (float)s_node_pool[c_idx].proof_depth;
        } else if (c_proof == MCTS_PROOF_WIN) {
            val = -10000.0f + (float)s_node_pool[c_idx].proof_depth;
        } else {
            uint32_t n = s_node_pool[c_idx].visits;
            float q = (n > 0) ? (s_node_pool[c_idx].wins / (float)n) : 0.0f;
            float p = s_node_pool[c_idx].prior;
            float u = c_puct * p * (sqrt_sum / (1.0f + (float)n));
            val = q + u;
        }

        if (val > best_val) {
            best_val = val;
            best_child = c_idx;
        }
    }

    return best_child;
}

// Expand node in PUCT tree and assign domain heuristic priors, opening book priors, and proof status
static bool puct_expand_node(uint32_t node_idx, const CompactState *state, const SearchHistory *hist, float temperature, bool use_book, bool use_db, TranspositionTable *tt, uint16_t epoch, uint16_t depth) {
    if (node_idx >= s_pool_tail) return false;
    if (s_node_pool[node_idx].first_child_idx != UINT32_MAX && s_node_pool[node_idx].first_child_idx < s_pool_tail) return true;

    const MoveList *ml_ptr = compact_get_valid_moves(state);
    if (!ml_ptr || ml_ptr->count == 0) {
        return false;
    }
    MoveList ml = *ml_ptr;
    s_node_pool[node_idx].num_children = ml.count;
    s_node_pool[node_idx].unexpanded_idx = 0;

    if (s_pool_tail + ml.count > PUCT_MAX_NODES) {
        return false;
    }

    uint32_t first_child = s_pool_tail;
    s_pool_tail += ml.count;
    s_node_pool[node_idx].first_child_idx = first_child;

    float priors[48];
    compute_priors(state, &ml, temperature, use_book, priors);

    for (uint8_t i = 0; i < ml.count; i++) {
        uint32_t child_idx = first_child + i;
        s_node_pool[child_idx].move = ml.moves[i];
        s_node_pool[child_idx].visits = 0;
        s_node_pool[child_idx].wins = 0.0f;
        s_node_pool[child_idx].prior = priors[i];
        s_node_pool[child_idx].parent_idx = node_idx;
        s_node_pool[child_idx].first_child_idx = UINT32_MAX;
        s_node_pool[child_idx].num_children = 0;
        s_node_pool[child_idx].unexpanded_idx = 0;
        s_node_pool[child_idx].proof_status = MCTS_PROOF_UNKNOWN;
        s_node_pool[child_idx].proof_depth = 0;

        CompactState child_state = *state;
        compact_execute_move(&child_state, ml.moves[i]);
        if (hist && compact_is_threefold_repetition(hist, child_state.hash)) {
            child_state.is_game_over = true;
            child_state.is_draw = true;
        }
        s_node_pool[child_idx].hash = child_state.hash;
        mcts_init_puct_node_proof(&s_node_pool[child_idx], state, &child_state, use_db);

        if (tt) {
            tt_store(tt, child_state.hash, child_idx, depth, epoch);
        }
    }
    mcts_update_puct_proof_status(node_idx);

    return true;
}

static inline bool node_is_fully_expanded(uint32_t idx) {
    if (s_node_pool[idx].first_child_idx == UINT32_MAX) return false;

    return s_node_pool[idx].num_children > 0 &&
           s_node_pool[idx].unexpanded_idx == s_node_pool[idx].num_children;
}

Move engine_mcts_puct_get_move(void *state, const GameState *game) {
    if (!state || !game || game->is_game_over) {
        return MOVE_NONE;
    }

    PUCTEngineState *st = (PUCTEngineState*)state;
    Player ai_player = game->current_player;

    pool_ensure();

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
        st->has_prev_state = false;
        st->root_idx = UINT32_MAX;
        st->prev_ai_move = MOVE_NONE;
        return root_valid_moves.moves[0];
    }


    // Direct Opening Book Mode: If enabled and in instant mode (BEST, GOOD, ALL), select directly (0 ms)!
    if (st->use_book && (st->book_mode == BOOK_MODE_BEST || st->book_mode == BOOK_MODE_GOOD || st->book_mode == BOOK_MODE_ALL)) {
        Move book_move = opening_book_select_move(game, st->book_mode, st->book_temperature, &st->rng_state);
        if (!move_is_none(book_move)) {
            if (st->debug_log) {
                printf("\n[MCTS PUCT] Instant Opening Book Move Played: %02d(r%d,c%d) -> %02d(r%d,c%d) (Mode: %d)\n\n",
                       MOVE_FROM(book_move), SQ_TO_ROW(MOVE_FROM(book_move)), SQ_TO_COL(MOVE_FROM(book_move)),
                       MOVE_TO(book_move), SQ_TO_ROW(MOVE_TO(book_move)), SQ_TO_COL(MOVE_TO(book_move)),
                       st->book_mode);
                fflush(stdout);
            }
            st->has_prev_state = false;
            st->root_idx = UINT32_MAX;
            st->prev_ai_move = MOVE_NONE;
            return book_move;
        }
    }

    // Safety threshold check: if pool is 80%+ full, reset to prevent overflow
    if (s_pool_tail >= PUCT_SAFETY_THRESHOLD_NODES || s_pool_owner != st) {
        pool_reset();
        s_pool_owner = st;
        st->root_idx = UINT32_MAX;
    }

    pool_ensure();

    // Subtree Promotion (Tree Reuse with 64-bit Zobrist Hash)
    bool tree_reused = false;
    if (s_pool_owner == st && st->has_prev_state && st->root_idx != UINT32_MAX && st->root_idx < s_pool_tail) {
        uint32_t ai_child_idx = UINT32_MAX;
        if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX && s_node_pool[st->root_idx].first_child_idx < s_pool_tail) {
            uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
            uint8_t nc = s_node_pool[st->root_idx].num_children;
            for (uint8_t i = 0; i < nc && (fc + i) < s_pool_tail; i++) {
                if (move_equals(s_node_pool[fc + i].move, st->prev_ai_move)) {
                    ai_child_idx = fc + i;
                    break;
                }
            }
        }

        if (ai_child_idx != UINT32_MAX && ai_child_idx < s_pool_tail &&
            s_node_pool[ai_child_idx].first_child_idx != UINT32_MAX && s_node_pool[ai_child_idx].first_child_idx < s_pool_tail) {
            uint32_t opp_fc = s_node_pool[ai_child_idx].first_child_idx;
            uint8_t opp_nc = s_node_pool[ai_child_idx].num_children;
            for (uint8_t j = 0; j < opp_nc && (opp_fc + j) < s_pool_tail; j++) {
                if (s_node_pool[opp_fc + j].hash == game->hash) {
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
        s_pool_owner = st;
        tt_clear(&st->tt);
        st->search_epoch = 1;
        st->root_idx = create_root_node(game->hash);
    } else {
        st->search_epoch++;
    }

    tt_store(&st->tt, game->hash, st->root_idx, 0, st->search_epoch);

    CompactState root_compact = compact_from_game(game);
    int irr_plies = game_get_plies_since_irreversible(game);
    SearchHistory search_hist;
    search_hist.root_history_len = (uint16_t)irr_plies;
    if (irr_plies > 0 && game->history_count > irr_plies) {
        search_hist.root_history = &game->history[game->history_count - 1 - irr_plies];
    } else {
        search_hist.root_history = NULL;
        search_hist.root_history_len = 0;
    }
    search_hist.path_len = 0;

    // Ensure root children are generated with domain heuristic priors, book priors, and tablebase proof status
    if (s_node_pool[st->root_idx].first_child_idx == UINT32_MAX) {
        puct_expand_node(st->root_idx, &root_compact, &search_hist, st->temperature, st->use_book, st->use_db, &st->tt, st->search_epoch, 1);

        // Root Tree Warm-Starting from Opening Book Metadata
        if (st->use_book) {
            BookMoveList root_book_moves;
            if (opening_book_probe_compact(&root_compact, &root_book_moves) && root_book_moves.count > 0) {
                uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
                uint8_t nc = s_node_pool[st->root_idx].num_children;
                for (uint8_t i = 0; i < nc; i++) {
                    uint32_t c_idx = fc + i;
                    Move cm = s_node_pool[c_idx].move;
                    for (int b = 0; b < root_book_moves.count; b++) {
                        if (move_equals(cm, root_book_moves.entries[b].move)) {
                            uint32_t n0 = (uint32_t)root_book_moves.entries[b].depth * 5;
                            if (n0 < 5) n0 = 5;
                            float q0 = 0.5f + ((float)root_book_moves.entries[b].score / 200.0f);
                            if (q0 < 0.01f) q0 = 0.01f;
                            if (q0 > 0.99f) q0 = 0.99f;
                            if (s_node_pool[c_idx].visits == 0) {
                                s_node_pool[c_idx].visits = n0;
                                s_node_pool[c_idx].wins = (float)n0 * q0;
                                s_node_pool[st->root_idx].visits += n0;
                                s_node_pool[st->root_idx].wins += (float)n0 * q0;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // High-performance Anytime Search Loop
    double start_time = puct_get_time();
    uint32_t iterations = 0;
    uint32_t path_stack[MAX_TREE_DEPTH];

    const float c_puct = st->c_puct;
    const double budget = st->time_budget;
    const float temperature = st->temperature;

    while (1) {
        if ((iterations & 511) == 0 && iterations > 0) {
            if (engine_is_stop_requested()) {
                break;
            }
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
        CompactState curr_state = root_compact;
        search_hist.path_len = 0;
        search_hist.path_hashes[search_hist.path_len++] = curr_state.hash;
        path_stack[path_len++] = curr_idx;

        while (s_node_pool[curr_idx].first_child_idx != UINT32_MAX && !curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            uint32_t best_child = puct_select_child(curr_idx, c_puct);
            if (best_child == UINT32_MAX) break;

            compact_execute_move(&curr_state, s_node_pool[best_child].move);
            if (compact_is_threefold_repetition(&search_hist, curr_state.hash)) {
                curr_state.is_game_over = true;
                curr_state.is_draw = true;
            }
            if (search_hist.path_len < 255) {
                search_hist.path_hashes[search_hist.path_len++] = curr_state.hash;
            }
            path_stack[path_len++] = best_child;
            curr_idx = best_child;

            // Probe TT for statistics
            tt_probe(&st->tt, curr_state.hash);

            // If an unvisited child or true terminal node is reached, stop selection and proceed to expansion/simulation
            if (s_node_pool[curr_idx].visits == 0 || curr_state.is_game_over) {
                break;
            }
        }

        // 2. EXPANSION (if reached node was previously visited, not expanded yet, and not terminal)
        if (!curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            if (s_node_pool[curr_idx].first_child_idx == UINT32_MAX) {
                if (puct_expand_node(curr_idx, &curr_state, &search_hist, temperature, st->use_book, st->use_db, &st->tt, st->search_epoch, (uint16_t)path_len)) {
                    // Pick top child by PUCT/prior to step into
                    uint32_t best_child = puct_select_child(curr_idx, c_puct);
                    if (best_child != UINT32_MAX) {
                        compact_execute_move(&curr_state, s_node_pool[best_child].move);
                        if (compact_is_threefold_repetition(&search_hist, curr_state.hash)) {
                            curr_state.is_game_over = true;
                            curr_state.is_draw = true;
                        }
                        if (search_hist.path_len < 255) {
                            search_hist.path_hashes[search_hist.path_len++] = curr_state.hash;
                        }
                        path_stack[path_len++] = best_child;
                        curr_idx = best_child;
                    }
                }
            }
        }

        // 3. SIMULATION (ROLLOUT - BIASED HEURISTIC POLICY)
        CompactState rollout_state = curr_state;
        int rollout_depth = 0;
        if (s_node_pool[curr_idx].proof_status == MCTS_PROOF_UNKNOWN && !rollout_state.is_game_over) {
            int max_depth = st->max_rollout_depth;
            float eps = st->rollout_epsilon;
            while (!rollout_state.is_game_over && rollout_depth < max_depth) {
                const MoveList *ml = compact_get_valid_moves(&rollout_state);
                if (!ml || ml->count == 0) {
                    rollout_state.is_game_over = true;
                    rollout_state.winner = (rollout_state.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                    break;
                }
                Move rm = mcts_select_biased_rollout_move_compact(&rollout_state, ml, eps, &st->rng_state);
                compact_execute_move(&rollout_state, rm);
                if (compact_is_threefold_repetition(&search_hist, rollout_state.hash)) {
                    rollout_state.is_game_over = true;
                    rollout_state.is_draw = true;
                    break;
                }
                if (search_hist.path_len < 255) {
                    search_hist.path_hashes[search_hist.path_len++] = rollout_state.hash;
                }
                rollout_depth++;
            }
        }

        // Calculate Reward: Bypass heuristic completely if the node is mathematically solved!
        float ai_reward;
        if (s_node_pool[curr_idx].proof_status != MCTS_PROOF_UNKNOWN) {
            if (s_node_pool[curr_idx].proof_status == MCTS_PROOF_DRAW) {
                ai_reward = 0.5f;
            } else if (s_node_pool[curr_idx].proof_status == MCTS_PROOF_WIN) {
                // The player to move at curr_idx can force a win
                ai_reward = (rollout_state.current_player == ai_player) ? 1.0f : 0.0f;
            } else { // MCTS_PROOF_LOSS
                // The player to move at curr_idx is forced to lose
                ai_reward = (rollout_state.current_player == ai_player) ? 0.0f : 1.0f;
            }
        } else {
            int total_depth = (path_len > 0 ? (path_len - 1) : 0) + rollout_depth;
            ai_reward = evaluate_rollout_terminal(&rollout_state, ai_player, total_depth);
        }

        // 4. BACKPROPAGATION (Values & Proof Updates)
        for (int p = path_len - 1; p >= 0; p--) {
            uint32_t n_idx = path_stack[p];
            s_node_pool[n_idx].visits += 1;

            if (p == 0) {
                s_node_pool[n_idx].wins += ai_reward;
            } else if (p % 2 == 1) {
                s_node_pool[n_idx].wins += ai_reward;
            } else {
                s_node_pool[n_idx].wins += (1.0f - ai_reward);
            }

            mcts_update_puct_proof_status(n_idx);
        }

        iterations++;
    }

    // ROBUST CHILD SELECTION WITH PROOF-NUMBER AWARENESS
    uint32_t best_child = UINT32_MAX;
    uint32_t max_visits = 0;
    float max_q = -1.0f;

    if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX) {
        uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
        uint8_t nc = s_node_pool[st->root_idx].num_children;

        // 1. If any child is a proven win (child proof_status == MCTS_PROOF_LOSS), choose the shortest win
        uint8_t min_win_depth = 255;
        uint32_t best_win_child = UINT32_MAX;
        for (uint8_t i = 0; i < nc; i++) {
            uint32_t c_idx = fc + i;
            if (s_node_pool[c_idx].proof_status == MCTS_PROOF_LOSS) {
                if (s_node_pool[c_idx].proof_depth < min_win_depth) {
                    min_win_depth = s_node_pool[c_idx].proof_depth;
                    best_win_child = c_idx;
                }
            }
        }

        if (best_win_child != UINT32_MAX) {
            best_child = best_win_child;
        } else {
            // 2. Otherwise, look for non-losing children with max visits
            for (uint8_t i = 0; i < nc; i++) {
                uint32_t c_idx = fc + i;
                if (s_node_pool[c_idx].proof_status == MCTS_PROOF_WIN) {
                    continue; // Skip proven losing moves
                }
                uint32_t v = s_node_pool[c_idx].visits;
                float q = (v > 0) ? (s_node_pool[c_idx].wins / (float)v) : 0.0f;
                if (v > max_visits || (v == max_visits && q > max_q)) {
                    max_visits = v;
                    max_q = q;
                    best_child = c_idx;
                }
            }

            // Fallback: if all children were proven losing, pick the one with max proof depth (longest resistance)
            if (best_child == UINT32_MAX) {
                uint8_t max_loss_depth = 0;
                for (uint8_t i = 0; i < nc; i++) {
                    uint32_t c_idx = fc + i;
                    if (s_node_pool[c_idx].proof_depth >= max_loss_depth) {
                        max_loss_depth = s_node_pool[c_idx].proof_depth;
                        best_child = c_idx;
                    }
                }
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
        printf("Parametri: c_puct = %.2f | Temp (tau) = %.2f | Epsilon Rollout = %.2f | Profondita Max Rollout = %d\n",
               st->c_puct, st->temperature, st->rollout_epsilon, st->max_rollout_depth);
        printf("Transposition Table: Occupazione = %u / %u (%.1f%%) | Hits = %u / %u (%.2f%%)\n",
               st->tt.count, st->tt.size,
               (float)st->tt.count * 100.0f / (float)st->tt.size,
               st->tt.hits, st->tt.lookups,
               tt_get_hit_rate(&st->tt));
        printf("--------------------------------------------------------------------------------\n");
        printf("  # | Mossa            | Prior P(s,a) | Visite (N)     | Win Rate (w/N) | Q      | Status / Proof\n");
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
                const char *note = (c_idx == best_child) ? " [*SCELTA*]" : "";
                char proof_buf[32] = "";
                if (s_node_pool[c_idx].proof_status == MCTS_PROOF_LOSS) {
                    snprintf(proof_buf, sizeof(proof_buf), "[PROVEN WIN d=%u]%s", s_node_pool[c_idx].proof_depth, note);
                } else if (s_node_pool[c_idx].proof_status == MCTS_PROOF_WIN) {
                    snprintf(proof_buf, sizeof(proof_buf), "[PROVEN LOSS d=%u]%s", s_node_pool[c_idx].proof_depth, note);
                } else if (s_node_pool[c_idx].proof_status == MCTS_PROOF_DRAW) {
                    snprintf(proof_buf, sizeof(proof_buf), "[PROVEN DRAW d=%u]%s", s_node_pool[c_idx].proof_depth, note);
                } else {
                    snprintf(proof_buf, sizeof(proof_buf), "%s", note);
                }

                printf(" %2d | %02d(r%d,c%d)->%02d(r%d,c%d) | %10.4f   | %7u (%5.1f%%) | %6.1f%%        | %6.4f | %s\n",
                       i + 1,
                       MOVE_FROM(cm), SQ_TO_ROW(MOVE_FROM(cm)), SQ_TO_COL(MOVE_FROM(cm)),
                       MOVE_TO(cm), SQ_TO_ROW(MOVE_TO(cm)), SQ_TO_COL(MOVE_TO(cm)),
                       p, v, pct, q * 100.0f, q, proof_buf);
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

    // Record stats
    double final_elapsed = puct_get_time() - start_time;
    st->last_stats.last_time = final_elapsed;
    st->last_stats.nodes_used = s_pool_tail;
    st->last_stats.nodes_max = PUCT_MAX_NODES;
    st->last_stats.iterations = iterations;
    st->last_stats.iterations_sec = (final_elapsed > 0.0001) ? ((double)iterations / final_elapsed) : 0.0;
    st->last_stats.win_rate = (max_q >= 0.0f) ? max_q : 0.5f;
    st->last_stats.is_valid = true;

    // Record for future subtree promotion
    st->has_prev_state = true;
    st->prev_game_state = *game;
    st->prev_ai_move = selected_move;

    return selected_move;
}

void engine_mcts_puct_get_stats(void *state, EngineStats *out_stats) {
    if (!state || !out_stats) return;
    PUCTEngineState *st = (PUCTEngineState*)state;
    *out_stats = st->last_stats;
}

