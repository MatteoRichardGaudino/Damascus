#ifndef MCTS_UCB1_H
#define MCTS_UCB1_H

#include "engine.h"
#include "transposition.h"
#include <stdint.h>
#include <stdbool.h>

#define MCTS_MAX_NODES              2000000
#define MCTS_SAFETY_THRESHOLD_NODES 1600000
#define MCTS_DEFAULT_EXPLORATION    1.41421356f
#define MCTS_DEFAULT_TIME_BUDGET    1.0
#define MCTS_MAX_ROLLOUT_DEPTH      70
#define MCTS_DEFAULT_ROLLOUT_EPSILON 0.15f

// Compact Node Structure using 32-bit indices and 64-bit Zobrist key
typedef struct {
    uint64_t hash;            // 64-bit Zobrist key dello stato
    uint32_t visits;          // N: numero totale di visite
    float    wins;            // w: reward totale accumulato
    uint32_t parent_idx;      // Indice del padre (UINT32_MAX se radice)
    uint32_t first_child_idx; // Indice del primo figlio nel pool (UINT32_MAX se non ancora espanso)
    Move     move;            // Mossa eseguita per raggiungere questo stato (from, to, flag)
    uint8_t  num_children;    // Numero totale di mosse legali generate
    uint8_t  unexpanded_idx;  // Quanti figli sono già stati espansi/simulati
} MCTSNode;

// Engine Lifecycle & Move API
void engine_mcts_ucb1_init(void **state);
Move engine_mcts_ucb1_get_move(void *state, const GameState *game);
void engine_mcts_ucb1_cleanup(void *state);

// Configuration API
void engine_mcts_ucb1_set_time_budget(void *state, double seconds);
void engine_mcts_ucb1_set_exploration(void *state, float alpha);
void engine_mcts_ucb1_set_max_rollout_depth(void *state, int depth);
void engine_mcts_ucb1_set_rollout_epsilon(void *state, float epsilon);
void engine_mcts_ucb1_set_use_db(void *state, bool enable);
void engine_mcts_ucb1_set_debug_log(void *state, bool enable);
uint32_t engine_mcts_ucb1_get_node_count(void);
uint32_t engine_mcts_ucb1_get_root_visits(void *state);


#endif // MCTS_UCB1_H

