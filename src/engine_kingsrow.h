#ifndef ENGINE_KINGSROW_H
#define ENGINE_KINGSROW_H

#include "engine.h"

void engine_kingsrow_init(void **state);
Move engine_kingsrow_get_move(void *state, const GameState *game);
void engine_kingsrow_cleanup(void *state);
bool engine_kingsrow_is_available(void);

#endif // ENGINE_KINGSROW_H
