#ifndef INTERACTION_H
#define INTERACTION_H

#include "game.h"
#include "camera.h"

bool interaction_pick_tile(double mouse_x, double mouse_y, int win_w, int win_h, const Camera *cam, int *out_row, int *out_col);

#endif // INTERACTION_H
