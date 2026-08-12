#include "camera.h"
#include <math.h>

void camera_init(Camera *cam) {
    cam->target[0] = 3.5f; // Center of 8x8 board
    cam->target[1] = 0.0f;
    cam->target[2] = 3.5f;
    
    cam->distance = 15.0f;
    cam->target_distance = 15.0f;
    cam->yaw = 45.0f;          // Start in diagonal orientation for menu background
    cam->target_yaw = 45.0f;
    cam->pitch = 40.0f;         // Optimal 3D depth tilt
    cam->fov = 45.0f;
    cam->is_rotating = false;
    cam->last_mouse_x = 0.0;
    cam->last_mouse_y = 0.0;
}

void camera_start_game_anim(Camera *cam, Player player) {
    cam->target_distance = 12.0f;
    
    // Smoothly rotate from diagonal (45 deg) to behind player's baseline
    if (player == PLAYER_BLACK) {
        cam->target_yaw = 0.0f;    // Directly behind Black's side
    } else {
        cam->target_yaw = 180.0f;  // Directly behind White's side
    }
}

void camera_reset_menu_anim(Camera *cam) {
    cam->target_distance = 15.0f;
    cam->target_yaw = 45.0f;       // Return to diagonal menu angle
}

void camera_update(Camera *cam, float delta_time) {
    if (delta_time <= 0.0f) return;
    
    // Smooth lerp for camera rotation & zoom sweep
    float lerp_factor = 6.0f * delta_time;
    if (lerp_factor > 1.0f) lerp_factor = 1.0f;
    
    cam->yaw += (cam->target_yaw - cam->yaw) * lerp_factor;
    cam->distance += (cam->target_distance - cam->distance) * lerp_factor;
}

void camera_update_rotation(Camera *cam, double delta_x, double delta_y) {
    float sensitivity = 0.3f;
    cam->yaw += (float)delta_x * sensitivity;
    cam->target_yaw = cam->yaw;
    
    cam->pitch += (float)delta_y * sensitivity;
    if (cam->pitch > 85.0f) cam->pitch = 85.0f;
    if (cam->pitch < 15.0f) cam->pitch = 15.0f;
}

void camera_update_zoom(Camera *cam, float offset) {
    cam->target_distance -= offset * 0.8f;
    if (cam->target_distance < 5.0f) cam->target_distance = 5.0f;
    if (cam->target_distance > 25.0f) cam->target_distance = 25.0f;
}

void camera_get_position(const Camera *cam, vec3 pos) {
    float rad_yaw = glm_rad(cam->yaw);
    float rad_pitch = glm_rad(cam->pitch);
    
    pos[0] = cam->target[0] + cam->distance * cosf(rad_pitch) * sinf(rad_yaw);
    pos[1] = cam->target[1] + cam->distance * sinf(rad_pitch);
    pos[2] = cam->target[2] + cam->distance * cosf(rad_pitch) * cosf(rad_yaw);
}

void camera_get_view_matrix(const Camera *cam, mat4 view) {
    vec3 eye;
    camera_get_position(cam, eye);
    vec3 up = { 0.0f, 1.0f, 0.0f };
    glm_lookat(eye, (float*)cam->target, up, view);
}

void camera_get_projection_matrix(const Camera *cam, float aspect_ratio, mat4 proj) {
    glm_perspective(glm_rad(cam->fov), aspect_ratio, 0.1f, 100.0f, proj);
}
