#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <glad/glad.h>
#include <cglm/cglm.h>
#include "game.h"
#include "camera.h"

#define MAX_JUMP_STEPS 8

typedef struct {
    bool active;
    
    // Multi-jump waypoint path for the moving piece
    int jump_count; // Number of intermediate jumps (1 for quiet move or single jump, 2..8 for multi-capture)
    int path_rows[MAX_JUMP_STEPS + 1];
    int path_cols[MAX_JUMP_STEPS + 1];
    PieceType piece_type;
    bool is_prom;
    
    // Sequential captured pieces fly-out
    int capture_count; // 0 for quiet move, 1..8 for capture
    int captured_rows[MAX_JUMP_STEPS];
    int captured_cols[MAX_JUMP_STEPS];
    PieceType captured_types[MAX_JUMP_STEPS];
    float captured_target_x[MAX_JUMP_STEPS];
    float captured_target_z[MAX_JUMP_STEPS];
    
    double start_time;
    double step_move_duration; // Duration for each individual jump arc (e.g. 0.22s)
    double step_cap_duration;  // Duration for each individual capture fly-out (e.g. 0.28s)
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

void graphics_render_scene(GraphicsContext *gfx, const GameState *game, const Camera *cam, float aspect_ratio, const MoveList *valid_moves, PieceAnim *anim, double current_time);
void piece_anim_start(PieceAnim *anim, const GameState *game, Move mv, double current_time);

#endif // GRAPHICS_H
