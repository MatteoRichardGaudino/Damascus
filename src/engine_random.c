#include "engine.h"
#include "engine_random.h"
#include "engine_checkerboard.h"
#include "engine_kingsrow.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    Move moves[128];
    int count = game_get_valid_moves(game, moves, 128);
    
    if (count == 0) {
        Move empty_move = { -1, -1, -1, -1, -1, -1, PIECE_NONE };
        return empty_move;
    }
    
    if (st) st->move_counter++;
    int index = rand() % count;
    return moves[index];
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
