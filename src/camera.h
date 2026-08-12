#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>
#include <stdbool.h>
#include "game.h"

typedef struct {
    vec3 target;
    float distance;
    float target_distance;
    float yaw;          // in degrees
    float target_yaw;   // target yaw orientation
    float pitch;        // in degrees (tilted for 3D depth)
    float fov;          // in degrees
    
    bool is_rotating;
    double last_mouse_x;
    double last_mouse_y;
} Camera;

void camera_init(Camera *cam);
void camera_start_game_anim(Camera *cam, Player player);
void camera_reset_menu_anim(Camera *cam);
void camera_update(Camera *cam, float delta_time);

void camera_update_rotation(Camera *cam, double delta_x, double delta_y);
void camera_update_zoom(Camera *cam, float offset);

void camera_get_view_matrix(const Camera *cam, mat4 view);
void camera_get_projection_matrix(const Camera *cam, float aspect_ratio, mat4 proj);
void camera_get_position(const Camera *cam, vec3 pos);

#endif // CAMERA_H
