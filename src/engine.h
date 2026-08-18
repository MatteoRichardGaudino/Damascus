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

// Engine & Wine availability checks
bool engine_is_wine_available(void);
bool engine_is_type_available(EngineType type);
const char *engine_get_type_name(EngineType type);

#endif // ENGINE_H
