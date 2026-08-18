#ifndef ENGINE_CHECKERBOARD_H
#define ENGINE_CHECKERBOARD_H

#include "engine.h"

void engine_checkerboard_init(void **state);
Move engine_checkerboard_get_move(void *state, const GameState *game);
void engine_checkerboard_cleanup(void *state);

#endif // ENGINE_CHECKERBOARD_H
