#ifndef ENGINE_H
#define ENGINE_H

#include "game.h"
#include "wld_db.h"
#include "opening_book.h"

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
    float  mcts_rollout_epsilon;   // Biased rollout exploration rate (e.g. 0.05, 0.15, 0.30, 1.0)
    bool   mcts_use_db;            // Enable/disable instant WLD tablebase mode (true/false)
    bool   mcts_use_book;          // Enable/disable opening book for MCTS UCB1 (true/false)
    bool   mcts_debug_log;         // Enable/disable verbose console debug logging for MCTS

    // MCTS PUCT parameters
    double puct_time_budget;       // Seconds (e.g. 0.2, 1.0, 3.0)
    float  puct_c_puct;            // Exploration constant c_puct (e.g. 1.5)
    float  puct_temperature;       // Softmax temperature tau (e.g. 1.0)
    int    puct_max_rollout_depth; // Max simulation depth (e.g. 20, 70, 150)
    float  puct_rollout_epsilon;   // Biased rollout exploration rate (e.g. 0.05, 0.15, 0.30, 1.0)
    bool   puct_use_db;            // Enable/disable instant WLD tablebase mode (true/false)
    bool   puct_use_book;          // Enable/disable opening book for MCTS PUCT (true/false)
    bool   puct_debug_log;         // Enable/disable verbose console debug logging for PUCT

    // Opening Book parameters
    BookBackendType book_backend;  // Selected book backend (KINGSROW_ODB, CHECKERBOARD_BIN, NONE)
    BookPlayMode    book_mode;     // Selected book mode (BEST, GOOD, ALL, PUCT_GUIDED, OFF)
    float           book_temperature; // Temperature for book move sampling
    char            book_custom_path[256]; // Optional custom path override

    // Endgame Tablebase (WLD) parameters
    WLDBackendType wld_backend;    // Selected WLD backend (OFFICIAL_8PIECE, REDUCED_NATIVE, NONE)
    char           wld_custom_path[256]; // Optional custom path override

    // CheckerBoard parameters
    double cb_search_time;         // Search time in seconds (e.g. 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0)

    // Kingsrow parameters
    double kr_search_time;         // Max search time in seconds (e.g. 0.5, 1.0, 2.0, 5.0, 10.0)
} EngineConfig;


typedef enum {
    TIME_PROFILE_CUSTOM = 0,
    TIME_PROFILE_FAST = 1,   // 0.2s (Fast profile)
    TIME_PROFILE_MEDIUM = 2, // 1.0s (Medium profile)
    TIME_PROFILE_SLOW = 3    // 3.0s (Slow profile)
} TimeProfile;

// Helper to create an engine instance by EngineType
Engine engine_create(EngineType type);
void engine_destroy(Engine *engine);
void engine_config_init_default(EngineConfig *cfg);
void engine_config_set_time_profile(EngineConfig *cfg, TimeProfile profile);
TimeProfile engine_config_get_time_profile(const EngineConfig *cfg);
void engine_apply_config(Engine *engine, EngineType type, const EngineConfig *cfg);

typedef struct {
    double last_time;       // Seconds taken in last search
    uint32_t nodes_used;    // Node pool allocated count
    uint32_t nodes_max;     // Max nodes capacity (e.g. 2000000)
    uint32_t iterations;    // Total simulations in last search
    double iterations_sec;  // Iterations / sec
    float win_rate;         // Win rate (0.0 to 1.0)
    bool is_valid;          // True if valid statistics
} EngineStats;

// Engine & Wine availability checks
bool engine_is_wine_available(void);
bool engine_is_type_available(EngineType type);
const char *engine_get_type_name(EngineType type);

// Engine live statistics and cancellation control
void engine_get_stats(const Engine *engine, EngineType type, EngineStats *out_stats);
void engine_request_stop(void);
void engine_reset_stop(void);
bool engine_is_stop_requested(void);

#endif // ENGINE_H

