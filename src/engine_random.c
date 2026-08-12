#include "engine_random.h"
#include <stdlib.h>
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
    Move moves[128];
    int count = game_get_valid_moves(game, moves, 128);
    
    if (count == 0) {
        Move empty_move = { -1, -1, -1, -1, -1, -1, PIECE_NONE };
        return empty_move;
    }
    
    // Use stateful seed update for reproducibility or pseudo-randomness
    st->move_counter++;
    int index = rand() % count;
    return moves[index];
}

void engine_random_cleanup(void *state) {
    if (state) {
        free(state);
    }
}

Engine engine_create(EngineType type) {
    Engine eng;
    eng.internal_state = NULL;
    switch (type) {
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
