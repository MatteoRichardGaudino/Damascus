#ifndef MCTS_PUCT_H
#define MCTS_PUCT_H

#include "engine.h"
#include "transposition.h"
#include "mcts_heuristic.h"
#include <stdint.h>
#include <stdbool.h>

#define PUCT_MAX_NODES              2000000
#define PUCT_SAFETY_THRESHOLD_NODES 1600000
#define PUCT_DEFAULT_C_PUCT         1.5f
#define PUCT_DEFAULT_TEMPERATURE    1.0f
#define PUCT_DEFAULT_TIME_BUDGET    1.0
#define PUCT_MAX_ROLLOUT_DEPTH      70
#define PUCT_DEFAULT_ROLLOUT_EPSILON 0.15f

// Compact Node Structure with prior probability P(s, a) and 64-bit Zobrist key
typedef struct {
    uint64_t hash;            // 64-bit Zobrist key
    uint32_t visits;          // N: total visits
    float    wins;            // w: accumulated reward
    float    prior;           // P(s, a): prior probability from domain heuristic policy
    uint32_t parent_idx;      // Parent node index (UINT32_MAX if root)
    uint32_t first_child_idx; // First child index in pool (UINT32_MAX if not expanded)
    Move     move;            // Move leading to this node
    uint8_t  num_children;    // Total legal moves generated
    uint8_t  unexpanded_idx;  // Unexpanded child tracker
    uint8_t  proof_status;    // MCTSProofStatus (UNKNOWN, WIN, LOSS, DRAW)
    uint8_t  proof_depth;     // Distanza in ply alla conclusione certa / mate
} PUCTNode;

// Engine Lifecycle & Move API
void engine_mcts_puct_init(void **state);
Move engine_mcts_puct_get_move(void *state, const GameState *game);
void engine_mcts_puct_cleanup(void *state);

// Configuration API
void engine_mcts_puct_set_time_budget(void *state, double seconds);
void engine_mcts_puct_set_c_puct(void *state, float c_puct);
void engine_mcts_puct_set_temperature(void *state, float tau);
void engine_mcts_puct_set_max_rollout_depth(void *state, int depth);
void engine_mcts_puct_set_rollout_epsilon(void *state, float epsilon);
void engine_mcts_puct_set_use_db(void *state, bool enable);
void engine_mcts_puct_set_use_book(void *state, bool enable);
void engine_mcts_puct_set_book_mode(void *state, BookPlayMode mode);
void engine_mcts_puct_set_book_temperature(void *state, float tau);
void engine_mcts_puct_set_debug_log(void *state, bool enable);
uint32_t engine_mcts_puct_get_node_count(void);
uint32_t engine_mcts_puct_get_root_visits(void *state);
void engine_mcts_puct_get_stats(void *state, EngineStats *out_stats);

// Fast domain heuristic function H(s, a)
float puct_compute_heuristic(const GameState *state, Move move);

#endif // MCTS_PUCT_H
