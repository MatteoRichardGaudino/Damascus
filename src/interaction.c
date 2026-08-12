#include "interaction.h"
#include <cglm/cglm.h>
#include <math.h>

bool interaction_pick_tile(double mouse_x, double mouse_y, int win_w, int win_h, const Camera *cam, int *out_row, int *out_col) {
    if (win_w <= 0 || win_h <= 0) return false;
    
    // Normalized Device Coordinates (-1 to 1)
    float x = (2.0f * (float)mouse_x) / (float)win_w - 1.0f;
    float y = 1.0f - (2.0f * (float)mouse_y) / (float)win_h;
    
    float aspect = (float)win_w / (float)win_h;
    
    mat4 view, proj;
    camera_get_view_matrix(cam, view);
    camera_get_projection_matrix(cam, aspect, proj);
    
    mat4 inv_proj, inv_view;
    glm_mat4_inv(proj, inv_proj);
    glm_mat4_inv(view, inv_view);
    
    // Clip space ray
    vec4 ray_clip = { x, y, -1.0f, 1.0f };
    
    // Eye space ray
    vec4 ray_eye;
    glm_mat4_mulv(inv_proj, ray_clip, ray_eye);
    ray_eye[2] = -1.0f;
    ray_eye[3] = 0.0f;
    
    // World space ray
    vec4 ray_world4;
    glm_mat4_mulv(inv_view, ray_eye, ray_world4);
    
    vec3 ray_dir = { ray_world4[0], ray_world4[1], ray_world4[2] };
    glm_vec3_normalize(ray_dir);
    
    vec3 ray_origin;
    camera_get_position(cam, ray_origin);
    
    // Intersect with horizontal board plane y = 0.1f
    float plane_y = 0.1f;
    if (fabsf(ray_dir[1]) < 0.0001f) return false; // Parallel to board
    
    float t = (plane_y - ray_origin[1]) / ray_dir[1];
    if (t < 0.0f) return false;
    
    float hit_x = ray_origin[0] + t * ray_dir[0];
    float hit_z = ray_origin[2] + t * ray_dir[2];
    
    int col = (int)floorf(hit_x + 0.5f);
    int row = (int)floorf(hit_z + 0.5f);
    
    if (game_is_valid_coord(row, col)) {
        *out_row = row;
        *out_col = col;
        return true;
    }
    
    return false;
}
