/*______________________________________________________________________________
  MCTS_UCB1 Italian Checkers Engine (Dama Italiana)
  High-Performance Monte Carlo Tree Search with UCB1 & Static Memory Pool
  Zero dynamic allocations during search, Anytime budget compliance, Subtree reuse
______________________________________________________________________________*/

#include "mcts_ucb1.h"
#include "mcts_heuristic.h"
#include "opening_book.h"
#include "wld_db.h"
#include "wld_solver.h"
#include "zobrist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_TREE_DEPTH 256

typedef struct {
    double             time_budget;
    float              exploration_alpha;
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
} MCTSEngineState;

// Per-thread dynamic preallocated memory pool of 2,000,000 nodes (~48 MB per active thread)
static _Thread_local MCTSNode *s_node_pool = NULL;
static _Thread_local uint32_t s_pool_tail = 0;
static _Thread_local void *s_pool_owner = NULL;

#ifdef _WIN32
#include <windows.h>
static inline double mcts_get_time(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
static inline double mcts_get_time(void) {
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
        s_node_pool = (MCTSNode*)malloc(sizeof(MCTSNode) * MCTS_MAX_NODES);
    }
}

static void pool_reset(void) {
    s_pool_tail = 0;
}

static uint32_t create_root_node(uint64_t hash) {
    pool_ensure();
    if (s_pool_tail >= MCTS_MAX_NODES) {
        pool_reset();
    }
    uint32_t idx = s_pool_tail++;
    s_node_pool[idx].hash = hash;
    s_node_pool[idx].visits = 0;
    s_node_pool[idx].wins = 0.0f;
    s_node_pool[idx].parent_idx = UINT32_MAX;
    s_node_pool[idx].first_child_idx = UINT32_MAX;
    s_node_pool[idx].move = MOVE_NONE;
    s_node_pool[idx].num_children = 0;
    s_node_pool[idx].unexpanded_idx = 0;
    s_node_pool[idx].proof_status = MCTS_PROOF_UNKNOWN;
    s_node_pool[idx].proof_depth = 0;
    return idx;
}

void engine_mcts_ucb1_init(void **state) {
    wld_db_init();
    opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    zobrist_init();
    MCTSEngineState *st = (MCTSEngineState*)malloc(sizeof(MCTSEngineState));
    if (!st) {
        *state = NULL;
        return;
    }
    memset(st, 0, sizeof(MCTSEngineState));
    st->time_budget = MCTS_DEFAULT_TIME_BUDGET;
    st->exploration_alpha = MCTS_DEFAULT_EXPLORATION;
    st->max_rollout_depth = MCTS_MAX_ROLLOUT_DEPTH;
    st->rollout_epsilon = MCTS_DEFAULT_ROLLOUT_EPSILON;
    st->use_db = true;
    st->use_book = true;
    st->book_mode = BOOK_MODE_BEST;
    st->book_temperature = 1.0f;
    st->debug_log = false;
    st->root_idx = UINT32_MAX;
    st->rng_state = (uint32_t)time(NULL) ^ 0x9E3779B9U;
    st->has_prev_state = false;
    st->prev_ai_move = MOVE_NONE;
    st->search_epoch = 1;
    tt_init(&st->tt, TT_DEFAULT_SIZE);
    *state = st;
}

void engine_mcts_ucb1_cleanup(void *state) {
    if (state) {
        MCTSEngineState *st = (MCTSEngineState*)state;
        tt_free(&st->tt);
        free(st);
    }
}

void engine_mcts_ucb1_set_time_budget(void *state, double seconds) {
    if (state && seconds > 0.0) {
        ((MCTSEngineState*)state)->time_budget = seconds;
    }
}

void engine_mcts_ucb1_set_exploration(void *state, float alpha) {
    if (state && alpha >= 0.0f) {
        ((MCTSEngineState*)state)->exploration_alpha = alpha;
    }
}

void engine_mcts_ucb1_set_max_rollout_depth(void *state, int depth) {
    if (state && depth > 0) {
        ((MCTSEngineState*)state)->max_rollout_depth = depth;
    }
}

void engine_mcts_ucb1_set_rollout_epsilon(void *state, float epsilon) {
    if (state && epsilon >= 0.0f && epsilon <= 1.0f) {
        ((MCTSEngineState*)state)->rollout_epsilon = epsilon;
    }
}

void engine_mcts_ucb1_set_use_db(void *state, bool enable) {
    if (state) {
        ((MCTSEngineState*)state)->use_db = enable;
    }
}

void engine_mcts_ucb1_set_use_book(void *state, bool enable) {
    if (state) {
        ((MCTSEngineState*)state)->use_book = enable;
    }
}

void engine_mcts_ucb1_set_book_mode(void *state, BookPlayMode mode) {
    if (state) {
        ((MCTSEngineState*)state)->book_mode = mode;
    }
}

void engine_mcts_ucb1_set_book_temperature(void *state, float tau) {
    if (state && tau > 0.0f) {
        ((MCTSEngineState*)state)->book_temperature = tau;
    }
}

void engine_mcts_ucb1_set_debug_log(void *state, bool enable) {
    if (state) {
        ((MCTSEngineState*)state)->debug_log = enable;
    }
}


uint32_t engine_mcts_ucb1_get_node_count(void) {
    return s_pool_tail;
}

uint32_t engine_mcts_ucb1_get_root_visits(void *state) {
    if (!state || !s_node_pool) return 0;
    MCTSEngineState *st = (MCTSEngineState*)state;
    if (st->root_idx == UINT32_MAX || st->root_idx >= s_pool_tail) return 0;
    return s_node_pool[st->root_idx].visits;
}


static bool boards_equal(const Board *a, const Board *b) {
    return (a->white_men == b->white_men) &&
           (a->white_kings == b->white_kings) &&
           (a->black_men == b->black_men) &&
           (a->black_kings == b->black_kings);
}

// Rollout evaluation cutoff with WLD tablebase integration and depth discounting
static float evaluate_rollout_terminal(const GameState *sim_state, Player ai_player, int total_depth) {
    if (sim_state->is_game_over) {
        return mcts_compute_depth_discounted_reward(
            !sim_state->is_draw && (sim_state->winner == ai_player),
            !sim_state->is_draw && (sim_state->winner != ai_player),
            sim_state->is_draw,
            total_depth
        );
    }

    // Exact Endgame Tablebase Probe (<= 5/8 pieces)
    if (wld_db_is_endgame(&sim_state->board)) {
        WLDValue wld = wld_db_probe(sim_state);
        if (wld == WLD_WIN_WHITE) {
            bool ai_wins = (ai_player == PLAYER_WHITE);
            return mcts_compute_depth_discounted_reward(ai_wins, !ai_wins, false, total_depth);
        } else if (wld == WLD_WIN_BLACK) {
            bool ai_wins = (ai_player == PLAYER_BLACK);
            return mcts_compute_depth_discounted_reward(ai_wins, !ai_wins, false, total_depth);
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

    bool ai_winning = (ai_player == PLAYER_WHITE) ? (w_score > b_score) : (b_score > w_score);
    return mcts_compute_depth_discounted_reward(ai_winning, !ai_winning, false, total_depth);
}

// Initialize node game-theoretic proof status from terminal state or tablebase probe
static inline void mcts_init_node_proof(MCTSNode *node, const GameState *state, bool use_db) {
    node->proof_status = MCTS_PROOF_UNKNOWN;
    node->proof_depth = 0;

    if (state->is_game_over) {
        if (state->is_draw) {
            node->proof_status = MCTS_PROOF_DRAW;
            node->proof_depth = 0;
        } else {
            node->proof_status = (state->winner == state->current_player) ? MCTS_PROOF_WIN : MCTS_PROOF_LOSS;
            node->proof_depth = 0;
        }
        return;
    }

    if (use_db && wld_db_is_endgame(&state->board)) {
        WLDValue wld = wld_db_probe(state);
        if (wld == WLD_WIN_WHITE) {
            node->proof_status = (state->current_player == PLAYER_WHITE) ? MCTS_PROOF_WIN : MCTS_PROOF_LOSS;
            node->proof_depth = 1;
        } else if (wld == WLD_WIN_BLACK) {
            node->proof_status = (state->current_player == PLAYER_BLACK) ? MCTS_PROOF_WIN : MCTS_PROOF_LOSS;
            node->proof_depth = 1;
        } else if (wld == WLD_DRAW) {
            node->proof_status = MCTS_PROOF_DRAW;
            node->proof_depth = 1;
        }
    }
}

// Update proof status of node based on child proof states (MCTS-Solver propagation)
static inline void mcts_update_proof_status(uint32_t node_idx) {
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

// Direct Endgame Database Move Selector via WLD Shortest-Win Mini-Solver
static Move mcts_select_database_move(const GameState *game, bool debug_log) {
    return wld_solver_select_move(game, WLD_SOLVER_DEFAULT_DEPTH, debug_log);
}

static inline bool node_is_fully_expanded(uint32_t idx) {
    if (s_node_pool[idx].first_child_idx == UINT32_MAX) return false;
    return s_node_pool[idx].num_children > 0 &&
           s_node_pool[idx].unexpanded_idx == s_node_pool[idx].num_children;
}


Move engine_mcts_ucb1_get_move(void *state, const GameState *game) {
    if (!state || !game || game->is_game_over) {
        return MOVE_NONE;
    }

    MCTSEngineState *st = (MCTSEngineState*)state;
    Player ai_player = game->current_player;

    // Check valid moves at current state
    MoveList root_valid_moves = *game_get_valid_moves(game);
    if (root_valid_moves.count == 0) {
        return MOVE_NONE;
    }
    if (root_valid_moves.count == 1) {
        if (st->debug_log) {
            Move fm = root_valid_moves.moves[0];
            printf("\n[MCTS DEBUG] Giocatore: %s | Mossa unica forzata: %02d(r%d,c%d) -> %02d(r%d,c%d)\n\n",
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

    // Direct Database Mode: If enabled and board is within tablebase endgame range, use shortest-win solver!
    if (st->use_db && wld_solver_is_applicable(game)) {
        Move db_move = mcts_select_database_move(game, st->debug_log);
        st->has_prev_state = true;
        st->prev_game_state = *game;
        st->prev_ai_move = db_move;
        return db_move;
    }

    // Direct Opening Book Mode: If enabled and in instant mode (BEST, GOOD, ALL), select directly (0 ms)!
    if (st->use_book && (st->book_mode == BOOK_MODE_BEST || st->book_mode == BOOK_MODE_GOOD || st->book_mode == BOOK_MODE_ALL)) {
        Move book_move = opening_book_select_move(game, st->book_mode, st->book_temperature, &st->rng_state);
        if (!move_is_none(book_move)) {
            if (st->debug_log) {
                printf("\n[MCTS UCB1] Instant Opening Book Move Played: %02d(r%d,c%d) -> %02d(r%d,c%d) (Mode: %d)\n\n",
                       MOVE_FROM(book_move), SQ_TO_ROW(MOVE_FROM(book_move)), SQ_TO_COL(MOVE_FROM(book_move)),
                       MOVE_TO(book_move), SQ_TO_ROW(MOVE_TO(book_move)), SQ_TO_COL(MOVE_TO(book_move)),
                       st->book_mode);
                fflush(stdout);
            }
            st->has_prev_state = true;
            st->prev_game_state = *game;
            st->prev_ai_move = book_move;
            return book_move;
        }
    }

    // Safety threshold check: if pool is 80%+ full or different instance is using pool, reset
    if (s_pool_tail >= MCTS_SAFETY_THRESHOLD_NODES || s_pool_owner != st) {
        pool_reset();
        s_pool_owner = st;
        st->root_idx = UINT32_MAX;
    }

    // Subtree Promotion (Tree Reuse with 64-bit Zobrist Hash)
    bool tree_reused = false;
    if (s_pool_owner == st && st->has_prev_state && st->root_idx != UINT32_MAX && st->root_idx < s_pool_tail) {
        // Find AI's previous move among old root's children
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

        // If AI's child node was found, look among its children for the opponent's response
        if (ai_child_idx != UINT32_MAX && s_node_pool[ai_child_idx].first_child_idx != UINT32_MAX) {
            uint32_t opp_fc = s_node_pool[ai_child_idx].first_child_idx;
            uint8_t opp_nc = s_node_pool[ai_child_idx].num_children;
            for (uint8_t j = 0; j < opp_nc; j++) {
                if (s_node_pool[opp_fc + j].hash == game->hash) {
                    // Subtree found via direct Zobrist 64-bit hash match! Promote to new root
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

    // Ensure root children are generated
    if (s_node_pool[st->root_idx].first_child_idx == UINT32_MAX) {
        s_node_pool[st->root_idx].num_children = root_valid_moves.count;
        s_node_pool[st->root_idx].unexpanded_idx = 0;
        if (root_valid_moves.count > 0 && s_pool_tail + root_valid_moves.count <= MCTS_MAX_NODES) {
            uint32_t start_c = s_pool_tail;
            s_pool_tail += root_valid_moves.count;
            s_node_pool[st->root_idx].first_child_idx = start_c;
            for (uint8_t i = 0; i < root_valid_moves.count; i++) {
                GameState child_st = *game;
                game_execute_move(&child_st, root_valid_moves.moves[i]);
                s_node_pool[start_c + i].hash = child_st.hash;
                s_node_pool[start_c + i].visits = 0;
                s_node_pool[start_c + i].wins = 0.0f;
                s_node_pool[start_c + i].parent_idx = st->root_idx;
                s_node_pool[start_c + i].first_child_idx = UINT32_MAX;
                s_node_pool[start_c + i].move = root_valid_moves.moves[i];
                s_node_pool[start_c + i].num_children = 0;
                s_node_pool[start_c + i].unexpanded_idx = 0;
                mcts_init_node_proof(&s_node_pool[start_c + i], &child_st, st->use_db);
                tt_store(&st->tt, child_st.hash, start_c + i, 1, st->search_epoch);
            }

            // Root Tree Warm-Starting from Opening Book Metadata
            if (st->use_book) {
                BookMoveList root_book_moves;
                if (opening_book_probe(game, &root_book_moves) && root_book_moves.count > 0) {
                    for (uint8_t i = 0; i < root_valid_moves.count; i++) {
                        uint32_t c_idx = start_c + i;
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

            mcts_update_proof_status(st->root_idx);
        }
    }

    // High-performance Anytime Search Loop
    double start_time = mcts_get_time();
    uint32_t iterations = 0;
    uint32_t path_stack[MAX_TREE_DEPTH];

    const float alpha = st->exploration_alpha;
    const double budget = st->time_budget;

    while (1) {
        // Non-blocking time and stop check strictly every 512 iterations
        if ((iterations & 511) == 0 && iterations > 0) {
            if (engine_is_stop_requested()) {
                break;
            }
            double elapsed = mcts_get_time() - start_time;
            if (elapsed >= budget) {
                break;
            }
            if (s_pool_tail + 64 >= MCTS_MAX_NODES) {
                break; // Near capacity
            }
        }

        // 1. SELECTION
        int path_len = 0;
        uint32_t curr_idx = st->root_idx;
        GameState curr_state = *game;
        path_stack[path_len++] = curr_idx;

        while (node_is_fully_expanded(curr_idx) && !curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            uint32_t fc = s_node_pool[curr_idx].first_child_idx;
            uint8_t nc = s_node_pool[curr_idx].num_children;
            if (nc == 0) break;

            uint32_t parent_n = s_node_pool[curr_idx].visits;
            float ln_parent = logf((float)(parent_n > 0 ? parent_n : 1));

            uint32_t best_child = fc;
            float best_ucb1 = -1e9f;

            for (uint8_t i = 0; i < nc; i++) {
                uint32_t c_idx = fc + i;
                uint8_t c_proof = s_node_pool[c_idx].proof_status;
                float val;

                if (c_proof == MCTS_PROOF_LOSS) {
                    // Child is proven loss for opponent -> proven WIN for current player
                    // Prioritize shortest win (smallest proof_depth)
                    val = 10000.0f - (float)s_node_pool[c_idx].proof_depth;
                } else if (c_proof == MCTS_PROOF_WIN) {
                    // Child is proven win for opponent -> proven LOSS for current player
                    // Strongly penalize losing branches
                    val = -10000.0f + (float)s_node_pool[c_idx].proof_depth;
                } else {
                    uint32_t c_n = s_node_pool[c_idx].visits;
                    float q = (c_n > 0) ? (s_node_pool[c_idx].wins / (float)c_n) : 0.0f;
                    float u = alpha * sqrtf((2.0f * ln_parent) / (float)(c_n > 0 ? c_n : 1));
                    val = q + u;
                }

                if (val > best_ucb1) {
                    best_ucb1 = val;
                    best_child = c_idx;
                }
            }

            game_execute_move(&curr_state, s_node_pool[best_child].move);
            curr_idx = best_child;
            path_stack[path_len++] = curr_idx;

            // Probe transposition table for statistics
            tt_probe(&st->tt, curr_state.hash);
        }

        // 2. EXPANSION
        uint32_t sim_node = curr_idx;
        if (!curr_state.is_game_over && path_len < MAX_TREE_DEPTH - 2) {
            if (s_node_pool[curr_idx].first_child_idx == UINT32_MAX) {
                MoveList ml = *game_get_valid_moves(&curr_state);
                s_node_pool[curr_idx].num_children = ml.count;
                s_node_pool[curr_idx].unexpanded_idx = 0;

                if (ml.count > 0 && s_pool_tail + ml.count <= MCTS_MAX_NODES) {
                    uint32_t start_c = s_pool_tail;
                    s_pool_tail += ml.count;
                    s_node_pool[curr_idx].first_child_idx = start_c;

                    for (uint8_t i = 0; i < ml.count; i++) {
                        GameState child_st = curr_state;
                        game_execute_move(&child_st, ml.moves[i]);
                        s_node_pool[start_c + i].hash = child_st.hash;
                        s_node_pool[start_c + i].visits = 0;
                        s_node_pool[start_c + i].wins = 0.0f;
                        s_node_pool[start_c + i].parent_idx = curr_idx;
                        s_node_pool[start_c + i].first_child_idx = UINT32_MAX;
                        s_node_pool[start_c + i].move = ml.moves[i];
                        s_node_pool[start_c + i].num_children = 0;
                        s_node_pool[start_c + i].unexpanded_idx = 0;
                        mcts_init_node_proof(&s_node_pool[start_c + i], &child_st, st->use_db);
                        tt_store(&st->tt, child_st.hash, start_c + i, (uint16_t)path_len, st->search_epoch);
                    }
                    mcts_update_proof_status(curr_idx);
                }
            }

            if (s_node_pool[curr_idx].first_child_idx != UINT32_MAX &&
                s_node_pool[curr_idx].unexpanded_idx < s_node_pool[curr_idx].num_children) {
                uint32_t child_to_expand = s_node_pool[curr_idx].first_child_idx + s_node_pool[curr_idx].unexpanded_idx;
                s_node_pool[curr_idx].unexpanded_idx++;

                game_execute_move(&curr_state, s_node_pool[child_to_expand].move);
                path_stack[path_len++] = child_to_expand;
                sim_node = child_to_expand;
            }
        }

        // 3. SIMULATION (ROLLOUT - BIASED HEURISTIC POLICY)
        GameState rollout_state = curr_state;
        int rollout_depth = 0;
        int max_depth = st->max_rollout_depth;
        float eps = st->rollout_epsilon;
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
            Move rm = mcts_select_biased_rollout_move(&rollout_state, &ml, eps, &st->rng_state);
            game_execute_move(&rollout_state, rm);
            rollout_depth++;
        }

        int total_depth = (path_len > 0 ? (path_len - 1) : 0) + rollout_depth;
        float ai_reward = evaluate_rollout_terminal(&rollout_state, ai_player, total_depth);

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

            mcts_update_proof_status(n_idx);
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
        double elapsed = mcts_get_time() - start_time;
        printf("\n================================================================================\n");
        printf("[MCTS DEBUG LOG - MONTE CARLO TREE SEARCH (UCB1)]\n");
        printf("Giocatore: %s | Mosse legali: %d | Nodi pool allocati: %u / %d\n",
               (ai_player == PLAYER_WHITE) ? "BIANCO" : "NERO", root_valid_moves.count, s_pool_tail, MCTS_MAX_NODES);
        printf("Budget tempo: %.2fs | Tempo impiegato: %.3fs | Simulazioni totali: %u\n",
               st->time_budget, elapsed, iterations);
        printf("Parametri: Alpha = %.2f | Epsilon Rollout = %.2f | Profondita Max Rollout = %d\n",
               st->exploration_alpha, st->rollout_epsilon, st->max_rollout_depth);
        printf("Transposition Table: Occupazione = %u / %u (%.1f%%) | Hits = %u / %u (%.2f%%)\n",
               st->tt.count, st->tt.size,
               (float)st->tt.count * 100.0f / (float)st->tt.size,
               st->tt.hits, st->tt.lookups,
               tt_get_hit_rate(&st->tt));
        printf("--------------------------------------------------------------------------------\n");
        printf("  # | Mossa            | Visite (N)     | Win Rate (w/N) | Punteggio / Q | Status / Proof\n");
        printf("--------------------------------------------------------------------------------\n");

        if (s_node_pool[st->root_idx].first_child_idx != UINT32_MAX) {
            uint32_t fc = s_node_pool[st->root_idx].first_child_idx;
            uint8_t nc = s_node_pool[st->root_idx].num_children;
            uint32_t total_visits = s_node_pool[st->root_idx].visits;
            for (uint8_t i = 0; i < nc; i++) {
                uint32_t c_idx = fc + i;
                Move cm = s_node_pool[c_idx].move;
                uint32_t v = s_node_pool[c_idx].visits;
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

                printf(" %2d | %02d(r%d,c%d)->%02d(r%d,c%d) | %7u (%5.1f%%) | %6.1f%%        | %7.4f       | %s\n",
                       i + 1,
                       MOVE_FROM(cm), SQ_TO_ROW(MOVE_FROM(cm)), SQ_TO_COL(MOVE_FROM(cm)),
                       MOVE_TO(cm), SQ_TO_ROW(MOVE_TO(cm)), SQ_TO_COL(MOVE_TO(cm)),
                       v, pct, q * 100.0f, q, proof_buf);
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
    double final_elapsed = mcts_get_time() - start_time;
    st->last_stats.last_time = final_elapsed;
    st->last_stats.nodes_used = s_pool_tail;
    st->last_stats.nodes_max = MCTS_MAX_NODES;
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

void engine_mcts_ucb1_get_stats(void *state, EngineStats *out_stats) {
    if (!state || !out_stats) return;
    MCTSEngineState *st = (MCTSEngineState*)state;
    *out_stats = st->last_stats;
}
