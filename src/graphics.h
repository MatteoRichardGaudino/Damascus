#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <glad/glad.h>
#include <cglm/cglm.h>
#include "game.h"
#include "camera.h"

typedef struct {
    bool active;
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    PieceType piece_type;
    
    // Sequential capture arc animation to side border
    bool has_capture;
    int captured_row;
    int captured_col;
    PieceType captured_type;
    float captured_target_x;
    float captured_target_z;
    
    double start_time;
    double move_duration;    // Stage 1: Piece move jump
    double capture_duration; // Stage 2: Captured piece fly-out to side (starts after move_duration)
} PieceAnim;

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLsizei index_count;
} Mesh;

typedef struct {
    GLuint basic_shader;
    GLuint ui_shader;
    
    Mesh cube_mesh;
    Mesh cylinder_mesh;
    
    // Shader Uniform Locations
    GLint u_model_loc;
    GLint u_view_loc;
    GLint u_proj_loc;
    GLint u_norm_mat_loc;
    GLint u_color_loc;
    GLint u_highlight_loc;
    GLint u_use_tex_loc;
    GLint u_light_pos_loc;
    GLint u_view_pos_loc;
} GraphicsContext;

bool graphics_init(GraphicsContext *gfx);
void graphics_cleanup(GraphicsContext *gfx);

void graphics_render_scene(GraphicsContext *gfx, const GameState *game, const Camera *cam, float aspect_ratio, int valid_move_count, const Move *valid_moves, PieceAnim *anim, double current_time);

#endif // GRAPHICS_H
