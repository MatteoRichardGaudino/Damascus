#ifndef ENGINE_H
#define ENGINE_H

#include "game.h"

typedef struct Engine {
    void *internal_state;
    void (*init)(void **state);
    Move (*get_move)(void *state, const GameState *game);
    void (*cleanup)(void *state);
} Engine;

// Helper to create an engine instance by EngineType
Engine engine_create(EngineType type);
void engine_destroy(Engine *engine);

#endif // ENGINE_H
