#include "engine.h"
#include "engine_random.h"
#include "engine_checkerboard.h"
#include "engine_kingsrow.h"
#include "mcts_ucb1.h"
#include "mcts_puct.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct {
    unsigned int seed;
    unsigned int move_counter;
} RandomEngineState;

void engine_random_init(void **state) {
    RandomEngineState *st = (RandomEngineState*)malloc(sizeof(RandomEngineState));
    if (st) {
        st->seed = (unsigned int)time(NULL);
        st->move_counter = 0;
    }
    *state = st;
}

Move engine_random_get_move(void *state, const GameState *game) {
    RandomEngineState *st = (RandomEngineState*)state;
    const MoveList *list = game_get_valid_moves(game);
    
    if (!list || list->count == 0) {
        return MOVE_NONE;
    }
    
    if (st) {
        st->move_counter++;
        st->seed ^= st->seed << 13;
        st->seed ^= st->seed >> 17;
        st->seed ^= st->seed << 5;
        int index = (int)(st->seed % list->count);
        return list->moves[index];
    }
    return list->moves[0];
}

void engine_random_cleanup(void *state) {
    if (state) {
        free(state);
    }
}

bool engine_is_wine_available(void) {
#ifdef _WIN32
    return true; // Native on Windows, wine not needed
#else
    static int cached_res = -1;
    if (cached_res != -1) return cached_res == 1;

    // Check for actual functional wine binary
    const char *wine_candidates[] = {
        "/usr/local/bin/wine",
        "/usr/local/bin/wine64",
        "/opt/homebrew/bin/wine",
        "/opt/homebrew/bin/wine64",
        "/usr/bin/wine",
        "/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine",
        "/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine"
    };

    for (size_t i = 0; i < sizeof(wine_candidates)/sizeof(wine_candidates[0]); i++) {
        if (access(wine_candidates[i], X_OK) == 0) {
            cached_res = 1;
            return true;
        }
    }

    cached_res = 0;
    return false;
#endif
}

bool engine_is_type_available(EngineType type) {
    switch (type) {
        case ENGINE_TYPE_RANDOM:
            return true;
        case ENGINE_TYPE_CHECKERBOARD:
            return true;
        case ENGINE_TYPE_KINGSROW:
            return engine_kingsrow_is_available();
        case ENGINE_TYPE_MCTS_UCB1:
            return true;
        case ENGINE_TYPE_MCTS_PUCT:
            return true;
        default:
            return false;
    }
}

const char *engine_get_type_name(EngineType type) {
    switch (type) {
        case ENGINE_TYPE_RANDOM:
            return "Random";
        case ENGINE_TYPE_CHECKERBOARD:
            return "CheckerBoard";
        case ENGINE_TYPE_KINGSROW:
            return "Kingsrow";
        case ENGINE_TYPE_MCTS_UCB1:
            return "MCTS_UCB1";
        case ENGINE_TYPE_MCTS_PUCT:
            return "MCTS_PUCT";
        default:
            return "Unknown";
    }
}

Engine engine_create(EngineType type) {
    Engine eng;
    memset(&eng, 0, sizeof(eng));

    switch (type) {
        case ENGINE_TYPE_CHECKERBOARD:
            eng.init = engine_checkerboard_init;
            eng.get_move = engine_checkerboard_get_move;
            eng.cleanup = engine_checkerboard_cleanup;
            break;
        case ENGINE_TYPE_KINGSROW:
            eng.init = engine_kingsrow_init;
            eng.get_move = engine_kingsrow_get_move;
            eng.cleanup = engine_kingsrow_cleanup;
            break;
        case ENGINE_TYPE_MCTS_UCB1:
            eng.init = engine_mcts_ucb1_init;
            eng.get_move = engine_mcts_ucb1_get_move;
            eng.cleanup = engine_mcts_ucb1_cleanup;
            break;
        case ENGINE_TYPE_MCTS_PUCT:
            eng.init = engine_mcts_puct_init;
            eng.get_move = engine_mcts_puct_get_move;
            eng.cleanup = engine_mcts_puct_cleanup;
            break;
        case ENGINE_TYPE_RANDOM:
        default:
            eng.init = engine_random_init;
            eng.get_move = engine_random_get_move;
            eng.cleanup = engine_random_cleanup;
            break;
    }

    if (eng.init) {
        eng.init(&eng.internal_state);
    }
    return eng;
}

void engine_destroy(Engine *engine) {
    if (engine && engine->cleanup && engine->internal_state) {
        engine->cleanup(engine->internal_state);
        engine->internal_state = NULL;
    }
}

void engine_config_init_default(EngineConfig *cfg) {
    if (!cfg) return;
    cfg->mcts_time_budget = 1.0;
    cfg->mcts_exploration = 1.41421356f;
    cfg->mcts_max_rollout_depth = 70;
    cfg->mcts_rollout_epsilon = 0.15f;
    cfg->mcts_use_db = true;
    cfg->mcts_use_book = true;
    cfg->mcts_debug_log = false;
    cfg->puct_time_budget = 1.0;
    cfg->puct_c_puct = 1.5f;
    cfg->puct_temperature = 1.0f;
    cfg->puct_max_rollout_depth = 70;
    cfg->puct_rollout_epsilon = 0.15f;
    cfg->puct_use_db = true;
    cfg->puct_use_book = true;
    cfg->puct_debug_log = false;
    cfg->book_backend = BOOK_BACKEND_KINGSROW_ODB;
    cfg->book_mode = BOOK_MODE_PUCT_GUIDED;
    cfg->book_temperature = 1.0f;
    cfg->book_custom_path[0] = '\0';
#ifdef _WIN32
    cfg->wld_backend = WLD_BACKEND_OFFICIAL_8PIECE;
#else
    cfg->wld_backend = WLD_BACKEND_REDUCED_NATIVE;
#endif
    cfg->wld_custom_path[0] = '\0';
    cfg->cb_search_time = 1.0;
    cfg->kr_search_time = 1.0;
}

void engine_config_set_time_profile(EngineConfig *cfg, TimeProfile profile) {
    if (!cfg) return;
    double t = 1.0;
    switch (profile) {
        case TIME_PROFILE_FAST:   t = 0.20; break;
        case TIME_PROFILE_MEDIUM: t = 1.00; break;
        case TIME_PROFILE_SLOW:   t = 3.00; break;
        case TIME_PROFILE_CUSTOM:
        default: return;
    }
    cfg->mcts_time_budget = t;
    cfg->puct_time_budget = t;
    cfg->cb_search_time = t;
    cfg->kr_search_time = t;
}

TimeProfile engine_config_get_time_profile(const EngineConfig *cfg) {
    if (!cfg) return TIME_PROFILE_CUSTOM;
    double t = cfg->mcts_time_budget;
    if (fabs(t - 0.20) < 0.05 && fabs(cfg->puct_time_budget - 0.20) < 0.05) return TIME_PROFILE_FAST;
    if (fabs(t - 1.00) < 0.05 && fabs(cfg->puct_time_budget - 1.00) < 0.05) return TIME_PROFILE_MEDIUM;
    if (fabs(t - 3.00) < 0.05 && fabs(cfg->puct_time_budget - 3.00) < 0.05) return TIME_PROFILE_SLOW;
    return TIME_PROFILE_CUSTOM;
}

void engine_apply_config(Engine *engine, EngineType type, const EngineConfig *cfg) {
    if (!engine || !engine->internal_state || !cfg) return;

    if (cfg->wld_custom_path[0] != '\0') {
        wld_set_custom_path(cfg->wld_custom_path);
    }
    if (cfg->wld_backend != WLD_BACKEND_NONE) {
        wld_init_backend(cfg->wld_backend);
    }

    if (cfg->book_backend != BOOK_BACKEND_NONE) {
        opening_book_init(cfg->book_backend, cfg->book_custom_path[0] ? cfg->book_custom_path : NULL);
    }

    switch (type) {
        case ENGINE_TYPE_MCTS_UCB1:
            engine_mcts_ucb1_set_time_budget(engine->internal_state, cfg->mcts_time_budget);
            engine_mcts_ucb1_set_exploration(engine->internal_state, cfg->mcts_exploration);
            engine_mcts_ucb1_set_max_rollout_depth(engine->internal_state, cfg->mcts_max_rollout_depth);
            engine_mcts_ucb1_set_rollout_epsilon(engine->internal_state, cfg->mcts_rollout_epsilon);
            engine_mcts_ucb1_set_use_db(engine->internal_state, (cfg->wld_backend != WLD_BACKEND_NONE) && cfg->mcts_use_db);
            engine_mcts_ucb1_set_use_book(engine->internal_state, cfg->mcts_use_book && (cfg->book_mode != BOOK_MODE_OFF));
            engine_mcts_ucb1_set_book_mode(engine->internal_state, cfg->book_mode);
            engine_mcts_ucb1_set_book_temperature(engine->internal_state, cfg->book_temperature);
            engine_mcts_ucb1_set_debug_log(engine->internal_state, cfg->mcts_debug_log);
            break;
        case ENGINE_TYPE_MCTS_PUCT:
            engine_mcts_puct_set_time_budget(engine->internal_state, cfg->puct_time_budget);
            engine_mcts_puct_set_c_puct(engine->internal_state, cfg->puct_c_puct);
            engine_mcts_puct_set_temperature(engine->internal_state, cfg->puct_temperature);
            engine_mcts_puct_set_max_rollout_depth(engine->internal_state, cfg->puct_max_rollout_depth);
            engine_mcts_puct_set_rollout_epsilon(engine->internal_state, cfg->puct_rollout_epsilon);
            engine_mcts_puct_set_use_db(engine->internal_state, (cfg->wld_backend != WLD_BACKEND_NONE) && cfg->puct_use_db);
            engine_mcts_puct_set_use_book(engine->internal_state, cfg->puct_use_book && (cfg->book_mode != BOOK_MODE_OFF));
            engine_mcts_puct_set_book_mode(engine->internal_state, cfg->book_mode);
            engine_mcts_puct_set_book_temperature(engine->internal_state, cfg->book_temperature);
            engine_mcts_puct_set_debug_log(engine->internal_state, cfg->puct_debug_log);
            break;
        case ENGINE_TYPE_CHECKERBOARD:
            engine_checkerboard_set_search_time(engine->internal_state, cfg->cb_search_time);
            break;
        case ENGINE_TYPE_KINGSROW:
            engine_kingsrow_set_search_time(engine->internal_state, cfg->kr_search_time);
            break;
        case ENGINE_TYPE_RANDOM:
        default:
            break;
    }
}

void engine_get_stats(const Engine *engine, EngineType type, EngineStats *out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(EngineStats));
    if (!engine || !engine->internal_state) return;

    switch (type) {
        case ENGINE_TYPE_MCTS_UCB1:
            engine_mcts_ucb1_get_stats(engine->internal_state, out_stats);
            break;
        case ENGINE_TYPE_MCTS_PUCT:
            engine_mcts_puct_get_stats(engine->internal_state, out_stats);
            break;
        case ENGINE_TYPE_CHECKERBOARD:
        case ENGINE_TYPE_KINGSROW:
        case ENGINE_TYPE_RANDOM:
        default:
            out_stats->is_valid = false;
            break;
    }
}
