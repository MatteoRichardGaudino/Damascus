/*______________________________________________________________________________
  MCTS_UCB1 Italian Checkers Engine (Dama Italiana)
  High-Performance Monte Carlo Tree Search with UCB1 & Static Memory Pool
  Zero dynamic allocations during search, Anytime budget compliance, Subtree reuse
______________________________________________________________________________*/

#include "mcts_ucb1.h"
#include "wld_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_TREE_DEPTH 256

typedef struct {
    double   time_budget;
    float    exploration_alpha;
    uint32_t root_idx;
    uint32_t rng_state;
    bool     has_prev_state;
    GameState prev_game_state;
    Move     prev_ai_move;
} MCTSEngineState;

// Static Memory Pool of 2,000,000 nodes (~40 MB in BSS)
static MCTSNode s_node_pool[MCTS_MAX_NODES];
static uint32_t s_pool_tail = 0;

static inline double mcts_get_time(void) {
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
    if (s_pool_tail >= MCTS_MAX_NODES) {
        pool_reset();
    }
    uint32_t idx = s_pool_tail++;
    s_node_pool[idx].visits = 0;
    s_node_pool[idx].wins = 0.0f;
    s_node_pool[idx].parent_idx = UINT32_MAX;
    s_node_pool[idx].first_child_idx = UINT32_MAX;
    s_node_pool[idx].move = MOVE_NONE;
    s_node_pool[idx].num_children = 0;
    s_node_pool[idx].unexpanded_idx = 0;
    return idx;
}

void engine_mcts_ucb1_init(void **state) {
    wld_db_init();
    MCTSEngineState *st = (MCTSEngineState*)malloc(sizeof(MCTSEngineState));
    if (!st) {
        *state = NULL;
        return;
    }
    memset(st, 0, sizeof(MCTSEngineState));
    st->time_budget = MCTS_DEFAULT_TIME_BUDGET;
    st->exploration_alpha = MCTS_DEFAULT_EXPLORATION;
    st->root_idx = UINT32_MAX;
    st->rng_state = (uint32_t)time(NULL) ^ 0x9E3779B9U;
    st->has_prev_state = false;
    st->prev_ai_move = MOVE_NONE;
    *state = st;
}

void engine_mcts_ucb1_cleanup(void *state) {
    if (state) {
        free(state);
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

uint32_t engine_mcts_ucb1_get_node_count(void) {
    return s_pool_tail;
}

uint32_t engine_mcts_ucb1_get_root_visits(void *state) {
    if (!state) return 0;
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

// Rollout evaluation cutoff with WLD tablebase integration
static float evaluate_rollout_terminal(const GameState *sim_state, Player ai_player) {
    if (sim_state->is_game_over) {
        if (sim_state->is_draw) {
            return 0.5f; // Draw by threefold repetition
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
        st->has_prev_state = true;
        st->prev_game_state = *game;
        st->prev_ai_move = root_valid_moves.moves[0];
        return root_valid_moves.moves[0];
    }

    // Safety threshold check: if pool is 80%+ full, reset to prevent overflow
    if (s_pool_tail >= MCTS_SAFETY_THRESHOLD_NODES) {
        pool_reset();
        st->root_idx = UINT32_MAX;
    }

    // Subtree Promotion (Tree Reuse)
    bool tree_reused = false;
    if (st->has_prev_state && st->root_idx != UINT32_MAX && st->root_idx < s_pool_tail) {
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
            GameState after_ai = st->prev_game_state;
            game_execute_move(&after_ai, st->prev_ai_move);

            uint32_t opp_fc = s_node_pool[ai_child_idx].first_child_idx;
            uint8_t opp_nc = s_node_pool[ai_child_idx].num_children;
            for (uint8_t j = 0; j < opp_nc; j++) {
                GameState test_state = after_ai;
                game_execute_move(&test_state, s_node_pool[opp_fc + j].move);
                if (boards_equal(&test_state.board, &game->board)) {
                    // Subtree found! Promote to new root
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

    // Ensure root children are generated
    if (s_node_pool[st->root_idx].first_child_idx == UINT32_MAX) {
        s_node_pool[st->root_idx].num_children = root_valid_moves.count;
        s_node_pool[st->root_idx].unexpanded_idx = 0;
        if (root_valid_moves.count > 0 && s_pool_tail + root_valid_moves.count <= MCTS_MAX_NODES) {
            uint32_t start_c = s_pool_tail;
            s_pool_tail += root_valid_moves.count;
            s_node_pool[st->root_idx].first_child_idx = start_c;
            for (uint8_t i = 0; i < root_valid_moves.count; i++) {
                s_node_pool[start_c + i].visits = 0;
                s_node_pool[start_c + i].wins = 0.0f;
                s_node_pool[start_c + i].parent_idx = st->root_idx;
                s_node_pool[start_c + i].first_child_idx = UINT32_MAX;
                s_node_pool[start_c + i].move = root_valid_moves.moves[i];
                s_node_pool[start_c + i].num_children = 0;
                s_node_pool[start_c + i].unexpanded_idx = 0;
            }
        }
    }

    // High-performance Anytime Search Loop
    double start_time = mcts_get_time();
    uint32_t iterations = 0;
    uint32_t path_stack[MAX_TREE_DEPTH];

    const float alpha = st->exploration_alpha;
    const double budget = st->time_budget;

    while (1) {
        // Non-blocking time check strictly every 1024 iterations
        if ((iterations & 1023) == 0 && iterations > 0) {
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
                uint32_t c_n = s_node_pool[c_idx].visits;
                float q = (c_n > 0) ? (s_node_pool[c_idx].wins / (float)c_n) : 0.0f;
                float u = alpha * sqrtf((2.0f * ln_parent) / (float)(c_n > 0 ? c_n : 1));
                float val = q + u;
                if (val > best_ucb1) {
                    best_ucb1 = val;
                    best_child = c_idx;
                }
            }

            game_execute_move(&curr_state, s_node_pool[best_child].move);
            curr_idx = best_child;
            path_stack[path_len++] = curr_idx;
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
                        s_node_pool[start_c + i].visits = 0;
                        s_node_pool[start_c + i].wins = 0.0f;
                        s_node_pool[start_c + i].parent_idx = curr_idx;
                        s_node_pool[start_c + i].first_child_idx = UINT32_MAX;
                        s_node_pool[start_c + i].move = ml.moves[i];
                        s_node_pool[start_c + i].num_children = 0;
                        s_node_pool[start_c + i].unexpanded_idx = 0;
                    }
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

        // 3. SIMULATION (ROLLOUT)
        GameState rollout_state = curr_state;
        int rollout_depth = 0;
        while (!rollout_state.is_game_over && rollout_depth < MCTS_MAX_ROLLOUT_DEPTH) {
            if (wld_db_is_endgame(&rollout_state.board)) {
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

            // Turn-relative reward perspective:
            // p = 0 (root): AI to move
            // p = 1: move made by AI -> reward is ai_reward
            // p = 2: move made by opponent -> reward is (1.0f - ai_reward)
            // in general: odd depth = ai_reward, even depth (>0) = (1.0f - ai_reward)
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

    // Record for future subtree promotion
    st->has_prev_state = true;
    st->prev_game_state = *game;
    st->prev_ai_move = selected_move;

    return selected_move;
}
