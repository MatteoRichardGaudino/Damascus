#ifndef ENGINE_H
#define ENGINE_H

#include "game.h"

typedef struct Engine {
    void *internal_state;
    void (*init)(void **state);
    Move (*get_move)(void *state, const GameState *game);
    void (*cleanup)(void *state);
} Engine;

typedef struct {
    // MCTS UCB1 parameters
    double mcts_time_budget;       // Seconds (e.g. 0.25, 0.5, 1.0, 2.0, 3.0, 5.0, 10.0)
    float  mcts_exploration;       // UCB1 Alpha (e.g. 0.5, 0.8, 1.0, 1.414, 1.8, 2.2, 2.8)
    int    mcts_max_rollout_depth; // Max simulation depth (e.g. 20, 35, 50, 70, 100, 150, 200)
    bool   mcts_use_db;            // Enable/disable instant WLD tablebase mode (true/false)
    bool   mcts_debug_log;         // Enable/disable verbose console debug logging for MCTS

    // MCTS PUCT parameters
    double puct_time_budget;       // Seconds (e.g. 0.2, 1.0, 3.0)
    float  puct_c_puct;            // Exploration constant c_puct (e.g. 1.5)
    float  puct_temperature;       // Softmax temperature tau (e.g. 1.0)
    int    puct_max_rollout_depth; // Max simulation depth (e.g. 20, 70, 150)
    bool   puct_use_db;            // Enable/disable instant WLD tablebase mode (true/false)
    bool   puct_debug_log;         // Enable/disable verbose console debug logging for PUCT

    // CheckerBoard parameters
    double cb_search_time;         // Search time in seconds (e.g. 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0)

    // Kingsrow parameters
    double kr_search_time;         // Max search time in seconds (e.g. 0.5, 1.0, 2.0, 5.0, 10.0)
} EngineConfig;


// Helper to create an engine instance by EngineType
Engine engine_create(EngineType type);
void engine_destroy(Engine *engine);
void engine_config_init_default(EngineConfig *cfg);
void engine_apply_config(Engine *engine, EngineType type, const EngineConfig *cfg);

// Engine & Wine availability checks
bool engine_is_wine_available(void);
bool engine_is_type_available(EngineType type);
const char *engine_get_type_name(EngineType type);

#endif // ENGINE_H

