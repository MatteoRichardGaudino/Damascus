#ifndef ENGINE_RANDOM_H
#define ENGINE_RANDOM_H

#include "engine.h"

void engine_random_init(void **state);
Move engine_random_get_move(void *state, const GameState *game);
void engine_random_cleanup(void *state);

#endif // ENGINE_RANDOM_H
