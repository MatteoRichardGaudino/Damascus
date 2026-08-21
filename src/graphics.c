#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static char* read_file_content(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("Failed to open shader file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = (char*)malloc(len + 1);
    if (buf) {
        fread(buf, 1, len, f);
        buf[len] = '\0';
    }
    fclose(f);
    return buf;
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    
    GLint status;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
        char info[512];
        glGetShaderInfoLog(s, 512, NULL, info);
        printf("Shader compile error: %s\n", info);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint load_shader_program(const char *vert_path, const char *frag_path) {
    char *v_src = read_file_content(vert_path);
    char *f_src = read_file_content(frag_path);
    if (!v_src || !f_src) {
        free(v_src);
        free(f_src);
        return 0;
    }
    
    GLuint v = compile_shader(GL_VERTEX_SHADER, v_src);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, f_src);
    free(v_src);
    free(f_src);
    
    if (!v || !f) return 0;
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    
    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        char info[512];
        glGetProgramInfoLog(prog, 512, NULL, info);
        printf("Shader link error: %s\n", info);
        glDeleteProgram(prog);
        return 0;
    }
    
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

static Mesh create_cube_mesh(void) {
    // Pos (3), Normal (3), UV (2)
    float vertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        // Right face
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        // Left face
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20  // Left
    };

    Mesh m;
    m.index_count = 36;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);

    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // UV
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return m;
}

static Mesh create_cylinder_mesh(int segments) {
    // Create cylinder of radius 0.4, height 0.2
    int vert_count = (segments + 1) * 4;
    float *verts = (float*)malloc(vert_count * 8 * sizeof(float));
    unsigned int *indices = (unsigned int*)malloc(segments * 12 * sizeof(unsigned int));
    
    float radius = 0.4f;
    float half_h = 0.1f;
    
    int v_idx = 0;
    int i_idx = 0;
    
    // Side vertices
    for (int i = 0; i <= segments; i++) {
        float theta = (float)i / segments * 2.0f * (float)M_PI;
        float x = cosf(theta);
        float z = sinf(theta);
        float u = (float)i / segments;
        
        // Bottom side
        verts[v_idx++] = x * radius; verts[v_idx++] = -half_h; verts[v_idx++] = z * radius;
        verts[v_idx++] = x;          verts[v_idx++] = 0.0f;    verts[v_idx++] = z;
        verts[v_idx++] = u;          verts[v_idx++] = 0.0f;
        
        // Top side
        verts[v_idx++] = x * radius; verts[v_idx++] = half_h;  verts[v_idx++] = z * radius;
        verts[v_idx++] = x;          verts[v_idx++] = 0.0f;    verts[v_idx++] = z;
        verts[v_idx++] = u;          verts[v_idx++] = 1.0f;
    }
    
    for (int i = 0; i < segments; i++) {
        int b0 = i * 2;
        int t0 = i * 2 + 1;
        int b1 = (i + 1) * 2;
        int t1 = (i + 1) * 2 + 1;
        
        indices[i_idx++] = b0; indices[i_idx++] = t0; indices[i_idx++] = t1;
        indices[i_idx++] = b0; indices[i_idx++] = t1; indices[i_idx++] = b1;
    }
    
    // Top cap center
    int top_center = (segments + 1) * 2;
    verts[v_idx++] = 0.0f; verts[v_idx++] = half_h; verts[v_idx++] = 0.0f;
    verts[v_idx++] = 0.0f; verts[v_idx++] = 1.0f;   verts[v_idx++] = 0.0f;
    verts[v_idx++] = 0.5f; verts[v_idx++] = 0.5f;
    
    for (int i = 0; i <= segments; i++) {
        float theta = (float)i / segments * 2.0f * (float)M_PI;
        float x = cosf(theta);
        float z = sinf(theta);
        
        verts[v_idx++] = x * radius; verts[v_idx++] = half_h; verts[v_idx++] = z * radius;
        verts[v_idx++] = 0.0f;       verts[v_idx++] = 1.0f;   verts[v_idx++] = 0.0f;
        verts[v_idx++] = x * 0.5f + 0.5f; verts[v_idx++] = z * 0.5f + 0.5f;
    }
    
    for (int i = 0; i < segments; i++) {
        indices[i_idx++] = top_center;
        indices[i_idx++] = top_center + 1 + i;
        indices[i_idx++] = top_center + 1 + i + 1;
    }

    Mesh m;
    m.index_count = i_idx;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v_idx * sizeof(float), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, i_idx * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    
    free(verts);
    free(indices);
    return m;
}

bool graphics_init(GraphicsContext *gfx) {
    gfx->basic_shader = load_shader_program("shaders/basic.vert", "shaders/basic.frag");
    gfx->ui_shader = load_shader_program("shaders/ui.vert", "shaders/ui.frag");
    
    if (!gfx->basic_shader || !gfx->ui_shader) {
        printf("Failed to load shaders\n");
        return false;
    }
    
    gfx->u_model_loc = glGetUniformLocation(gfx->basic_shader, "uModel");
    gfx->u_view_loc = glGetUniformLocation(gfx->basic_shader, "uView");
    gfx->u_proj_loc = glGetUniformLocation(gfx->basic_shader, "uProjection");
    gfx->u_norm_mat_loc = glGetUniformLocation(gfx->basic_shader, "uNormalMatrix");
    gfx->u_color_loc = glGetUniformLocation(gfx->basic_shader, "uColor");
    gfx->u_highlight_loc = glGetUniformLocation(gfx->basic_shader, "uHighlightColor");
    gfx->u_use_tex_loc = glGetUniformLocation(gfx->basic_shader, "uUseTexture");
    gfx->u_light_pos_loc = glGetUniformLocation(gfx->basic_shader, "uLightPos");
    gfx->u_view_pos_loc = glGetUniformLocation(gfx->basic_shader, "uViewPos");

    gfx->cube_mesh = create_cube_mesh();
    gfx->cylinder_mesh = create_cylinder_mesh(32);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    return true;
}

void graphics_cleanup(GraphicsContext *gfx) {
    glDeleteVertexArrays(1, &gfx->cube_mesh.vao);
    glDeleteBuffers(1, &gfx->cube_mesh.vbo);
    glDeleteBuffers(1, &gfx->cube_mesh.ebo);

    glDeleteVertexArrays(1, &gfx->cylinder_mesh.vao);
    glDeleteBuffers(1, &gfx->cylinder_mesh.vbo);
    glDeleteBuffers(1, &gfx->cylinder_mesh.ebo);

    glDeleteProgram(gfx->basic_shader);
    glDeleteProgram(gfx->ui_shader);
}

static void draw_cube(GraphicsContext *gfx, mat4 model, vec4 color, vec4 highlight) {
    mat3 norm_mat;
    glm_mat4_pick3(model, norm_mat);
    glm_mat3_inv(norm_mat, norm_mat);
    glm_mat3_transpose(norm_mat);
    
    glUniformMatrix4fv(gfx->u_model_loc, 1, GL_FALSE, (float*)model);
    glUniformMatrix3fv(gfx->u_norm_mat_loc, 1, GL_FALSE, (float*)norm_mat);
    glUniform4fv(gfx->u_color_loc, 1, color);
    glUniform4fv(gfx->u_highlight_loc, 1, highlight);
    glUniform1i(gfx->u_use_tex_loc, 0);

    glBindVertexArray(gfx->cube_mesh.vao);
    glDrawElements(GL_TRIANGLES, gfx->cube_mesh.index_count, GL_UNSIGNED_INT, 0);
}

static void draw_piece(GraphicsContext *gfx, mat4 model, vec4 color, vec4 highlight, bool is_dama) {
    mat3 norm_mat;
    glm_mat4_pick3(model, norm_mat);
    glm_mat3_inv(norm_mat, norm_mat);
    glm_mat3_transpose(norm_mat);

    glUniformMatrix4fv(gfx->u_model_loc, 1, GL_FALSE, (float*)model);
    glUniformMatrix3fv(gfx->u_norm_mat_loc, 1, GL_FALSE, (float*)norm_mat);
    glUniform4fv(gfx->u_color_loc, 1, color);
    glUniform4fv(gfx->u_highlight_loc, 1, highlight);
    glUniform1i(gfx->u_use_tex_loc, 0);

    glBindVertexArray(gfx->cylinder_mesh.vao);
    glDrawElements(GL_TRIANGLES, gfx->cylinder_mesh.index_count, GL_UNSIGNED_INT, 0);

    if (is_dama) {
        // Draw second cylinder stacked on top for Dama
        mat4 dama_model;
        glm_mat4_copy(model, dama_model);
        glm_translate(dama_model, (vec3){0.0f, 0.22f, 0.0f});
        
        glm_mat4_pick3(dama_model, norm_mat);
        glm_mat3_inv(norm_mat, norm_mat);
        glm_mat3_transpose(norm_mat);
        
        glUniformMatrix4fv(gfx->u_model_loc, 1, GL_FALSE, (float*)dama_model);
        glUniformMatrix3fv(gfx->u_norm_mat_loc, 1, GL_FALSE, (float*)norm_mat);
        glDrawElements(GL_TRIANGLES, gfx->cylinder_mesh.index_count, GL_UNSIGNED_INT, 0);
    }
}

void piece_anim_start(PieceAnim *anim, const GameState *game, Move mv, double current_time) {
    if (!anim) return;
    memset(anim, 0, sizeof(PieceAnim));
    
    anim->active = true;
    anim->start_time = current_time;
    anim->step_move_duration = 0.22;
    anim->step_cap_duration = 0.28;
    anim->piece_type = board_get_piece_at(&game->board, mv.from);
    anim->is_prom = mv.is_prom;
    
    if (!MOVE_IS_CAP(mv)) {
        anim->jump_count = 1;
        anim->path_rows[0] = SQ_TO_ROW(mv.from);
        anim->path_cols[0] = SQ_TO_COL(mv.from);
        anim->path_rows[1] = SQ_TO_ROW(mv.to);
        anim->path_cols[1] = SQ_TO_COL(mv.to);
        anim->capture_count = 0;
    } else {
        int jumps = mv.jumps > 0 ? mv.jumps : 1;
        if (jumps > MAX_JUMP_STEPS) jumps = MAX_JUMP_STEPS;
        anim->jump_count = jumps;
        
        // Waypoint path
        if (mv.jumps > 0) {
            for (int i = 0; i <= jumps; i++) {
                int sq = mv.path[i];
                anim->path_rows[i] = SQ_TO_ROW(sq);
                anim->path_cols[i] = SQ_TO_COL(sq);
            }
        } else {
            anim->path_rows[0] = SQ_TO_ROW(mv.from);
            anim->path_cols[0] = SQ_TO_COL(mv.from);
            anim->path_rows[1] = SQ_TO_ROW(mv.to);
            anim->path_cols[1] = SQ_TO_COL(mv.to);
        }
        
        anim->capture_count = jumps;
        int white_eaten_base = game->white_eaten_count;
        int black_eaten_base = game->black_eaten_count;
        
        for (int j = 0; j < jumps; j++) {
            int cap_sq = (mv.jumps > 0) ? mv.caps[j] : GET_CAPTURED_SQ(mv.from, mv.to);
            anim->captured_rows[j] = SQ_TO_ROW(cap_sq);
            anim->captured_cols[j] = SQ_TO_COL(cap_sq);
            PieceType cap_type = board_get_piece_at(&game->board, cap_sq);
            anim->captured_types[j] = cap_type;
            
            if (is_piece_black(cap_type)) {
                anim->captured_target_x[j] = -1.5f;
                anim->captured_target_z[j] = (float)(white_eaten_base++) * 0.65f;
            } else {
                anim->captured_target_x[j] = 8.5f;
                anim->captured_target_z[j] = (float)(black_eaten_base++) * 0.65f;
            }
        }
    }
}

void graphics_render_scene(GraphicsContext *gfx, const GameState *game, const Camera *cam, float aspect_ratio, const MoveList *valid_moves, PieceAnim *anim, double current_time) {
    glUseProgram(gfx->basic_shader);
    
    mat4 view, proj;
    camera_get_view_matrix(cam, view);
    camera_get_projection_matrix(cam, aspect_ratio, proj);
    
    vec3 cam_pos;
    camera_get_position(cam, cam_pos);
    vec3 light_pos = { 3.5f, 15.0f, 3.5f };
    
    glUniformMatrix4fv(gfx->u_view_loc, 1, GL_FALSE, (float*)view);
    glUniformMatrix4fv(gfx->u_proj_loc, 1, GL_FALSE, (float*)proj);
    glUniform3fv(gfx->u_light_pos_loc, 1, light_pos);
    glUniform3fv(gfx->u_view_pos_loc, 1, cam_pos);

    vec4 light_tile_color = { 0.88f, 0.82f, 0.72f, 1.0f }; // Warm cream
    vec4 dark_tile_color  = { 0.28f, 0.20f, 0.15f, 1.0f }; // Rich dark mahogany
    vec4 wood_frame_color = { 0.18f, 0.12f, 0.08f, 1.0f };
    vec4 side_border_color= { 0.22f, 0.16f, 0.11f, 1.0f };
    
    vec4 white_piece_color = { 0.95f, 0.92f, 0.85f, 1.0f };
    vec4 black_piece_color = { 0.12f, 0.12f, 0.14f, 1.0f };
    
    vec4 no_highlight = { 0.0f, 0.0f, 0.0f, 0.0f };
    vec4 select_highlight = { 0.95f, 0.75f, 0.1f, 0.6f };  // Warm Gold
    vec4 move_target_highlight = { 0.1f, 0.85f, 0.4f, 0.5f }; // Emerald Green
    vec4 move_anim_highlight = { 0.2f, 0.6f, 1.0f, 0.4f }; // Cyan Motion Glow

    // 1. Draw Wooden Frame & Base
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){3.5f, -0.4f, 3.5f});
    glm_scale(model, (vec3){12.0f, 0.4f, 10.0f});
    draw_cube(gfx, model, wood_frame_color, no_highlight);

    // Side Borders for eaten pieces
    glm_mat4_identity(model);
    glm_translate(model, (vec3){-1.5f, -0.1f, 3.5f});
    glm_scale(model, (vec3){1.6f, 0.2f, 8.4f});
    draw_cube(gfx, model, side_border_color, no_highlight);

    glm_mat4_identity(model);
    glm_translate(model, (vec3){8.5f, -0.1f, 3.5f});
    glm_scale(model, (vec3){1.6f, 0.2f, 8.4f});
    draw_cube(gfx, model, side_border_color, no_highlight);

    // 2. Draw 8x8 Board Tiles
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            glm_mat4_identity(model);
            glm_translate(model, (vec3){(float)c, 0.0f, (float)r});
            glm_scale(model, (vec3){0.98f, 0.2f, 0.98f});
            
            vec4 highlight;
            glm_vec4_copy(no_highlight, highlight);
            
            // Check if this tile is a valid move target
            if (game->selected_row >= 0 && game->selected_col >= 0 && game_is_dark_tile(r, c)) {
                int sel_sq = ROW_COL_TO_SQ(game->selected_row, game->selected_col);
                int target_sq = ROW_COL_TO_SQ(r, c);
                if (valid_moves) {
                    for (int m = 0; m < valid_moves->count; m++) {
                        Move mv = valid_moves->moves[m];
                        if (MOVE_FROM(mv) == sel_sq && MOVE_TO(mv) == target_sq) {
                            highlight[0] = move_target_highlight[0];
                            highlight[1] = move_target_highlight[1];
                            highlight[2] = move_target_highlight[2];
                            highlight[3] = move_target_highlight[3];
                            break;
                        }
                    }
                }
            }
            
            vec4 *t_col = game_is_dark_tile(r, c) ? &dark_tile_color : &light_tile_color;
            draw_cube(gfx, model, *t_col, highlight);
        }
    }

    // Update piece move & capture animation progress
    bool is_animating_move = false;
    float anim_x = 0.0f, anim_y = 0.2f, anim_z = 0.0f;
    
    bool is_animating_capture = false;
    float cap_x = 0.0f, cap_y = 0.2f, cap_z = 0.0f;
    int current_flying_cap_idx = -1;
    
    double total_move_duration = 0.0;
    double total_cap_duration = 0.0;
    double elapsed = 0.0;
    
    if (anim && anim->active) {
        total_move_duration = (double)anim->jump_count * anim->step_move_duration;
        total_cap_duration = (double)anim->capture_count * anim->step_cap_duration;
        double total_duration = total_move_duration + total_cap_duration;
        
        elapsed = current_time - anim->start_time;
        if (elapsed >= total_duration) {
            anim->active = false;
        } else if (elapsed < total_move_duration) {
            // Stage 1: Moving piece jumps through each waypoint in the path
            is_animating_move = true;
            int step = (int)(elapsed / anim->step_move_duration);
            if (step >= anim->jump_count) step = anim->jump_count - 1;
            
            double step_elapsed = elapsed - (double)step * anim->step_move_duration;
            float step_t = (float)(step_elapsed / anim->step_move_duration);
            if (step_t > 1.0f) step_t = 1.0f;
            float smooth_step_t = step_t * step_t * (3.0f - 2.0f * step_t);
            
            int f_row = anim->path_rows[step];
            int f_col = anim->path_cols[step];
            int t_row = anim->path_rows[step + 1];
            int t_col = anim->path_cols[step + 1];
            
            anim_x = (float)f_col + ((float)t_col - (float)f_col) * smooth_step_t;
            anim_z = (float)f_row + ((float)t_row - (float)f_row) * smooth_step_t;
            anim_y = 0.2f + sinf(smooth_step_t * (float)M_PI) * 0.5f;
        } else {
            // Stage 2: Captured pieces fly out one by one in chronological capture order
            double cap_elapsed = elapsed - total_move_duration;
            int cap_idx = (int)(cap_elapsed / anim->step_cap_duration);
            if (cap_idx < anim->capture_count) {
                is_animating_capture = true;
                current_flying_cap_idx = cap_idx;
                
                double this_cap_elapsed = cap_elapsed - (double)cap_idx * anim->step_cap_duration;
                float cap_t = (float)(this_cap_elapsed / anim->step_cap_duration);
                if (cap_t > 1.0f) cap_t = 1.0f;
                float smooth_cap_t = cap_t * cap_t * (3.0f - 2.0f * cap_t);
                
                int c_row = anim->captured_rows[cap_idx];
                int c_col = anim->captured_cols[cap_idx];
                float target_x = anim->captured_target_x[cap_idx];
                float target_z = anim->captured_target_z[cap_idx];
                
                cap_x = (float)c_col + (target_x - (float)c_col) * smooth_cap_t;
                cap_z = (float)c_row + (target_z - (float)c_row) * smooth_cap_t;
                cap_y = 0.2f + sinf(smooth_cap_t * (float)M_PI) * 1.3f;
            }
        }
    }

    // 3. Draw Pieces on Board
    for (int sq = 0; sq < 32; sq++) {
        int r = SQ_TO_ROW(sq);
        int c = SQ_TO_COL(sq);
        
        // Hide static piece at final destination while moving piece is jumping (Stage 1)
        if (is_animating_move && anim && anim->active) {
            int dest_row = anim->path_rows[anim->jump_count];
            int dest_col = anim->path_cols[anim->jump_count];
            if (r == dest_row && c == dest_col) {
                continue;
            }
        }
        
        // Check if this square is one of the captured pieces from active animation
        int cap_match_idx = -1;
        if (anim && anim->active && anim->capture_count > 0) {
            for (int j = 0; j < anim->capture_count; j++) {
                if (r == anim->captured_rows[j] && c == anim->captured_cols[j]) {
                    cap_match_idx = j;
                    break;
                }
            }
        }
        
        PieceType p = PIECE_NONE;
        if (cap_match_idx >= 0) {
            if (elapsed < total_move_duration) {
                // During Stage 1 (jumps): captured piece is still sitting on its square
                p = anim->captured_types[cap_match_idx];
            } else {
                // During Stage 2 (captures fly-out):
                // If it's waiting for its turn (j > current_flying_cap_idx), keep rendering it on board
                if (cap_match_idx > current_flying_cap_idx) {
                    p = anim->captured_types[cap_match_idx];
                } else {
                    // If it is flying or already flown out, do not draw statically on board
                    continue;
                }
            }
        } else {
            p = board_get_piece_at(&game->board, sq);
        }
        
        if (p == PIECE_NONE) continue;
        
        glm_mat4_identity(model);
        glm_translate(model, (vec3){(float)c, 0.2f, (float)r});
        
        vec4 *p_col = is_piece_white(p) ? &white_piece_color : &black_piece_color;
        bool is_selected = (r == game->selected_row && c == game->selected_col);
        bool is_dama = is_piece_dama(p);
        
        draw_piece(gfx, model, *p_col, is_selected ? select_highlight : no_highlight, is_dama);
    }

    // Draw active moving piece
    if (is_animating_move && anim) {
        glm_mat4_identity(model);
        glm_translate(model, (vec3){anim_x, anim_y, anim_z});
        vec4 *p_col = is_piece_white(anim->piece_type) ? &white_piece_color : &black_piece_color;
        bool is_dama = is_piece_dama(anim->piece_type);
        draw_piece(gfx, model, *p_col, move_anim_highlight, is_dama);
    }

    // Draw active captured flying piece
    if (is_animating_capture && anim && current_flying_cap_idx >= 0) {
        glm_mat4_identity(model);
        glm_translate(model, (vec3){cap_x, cap_y, cap_z});
        PieceType cap_type = anim->captured_types[current_flying_cap_idx];
        vec4 *p_col = is_piece_white(cap_type) ? &white_piece_color : &black_piece_color;
        bool is_dama = is_piece_dama(cap_type);
        draw_piece(gfx, model, *p_col, select_highlight, is_dama);
    }

    // 4. Draw Eaten Pieces on Side Borders (only draw pieces that have completed their fly-out)
    int white_eaten_draw = game->white_eaten_count;
    int black_eaten_draw = game->black_eaten_count;
    
    if (anim && anim->active && anim->capture_count > 0) {
        int completed_caps = 0;
        if (elapsed >= total_move_duration) {
            double cap_elapsed = elapsed - total_move_duration;
            completed_caps = (int)(cap_elapsed / anim->step_cap_duration);
            if (completed_caps > anim->capture_count) completed_caps = anim->capture_count;
        }
        
        for (int k = completed_caps; k < anim->capture_count; k++) {
            PieceType ct = anim->captured_types[k];
            if (is_piece_black(ct) && white_eaten_draw > 0) {
                white_eaten_draw--;
            } else if (is_piece_white(ct) && black_eaten_draw > 0) {
                black_eaten_draw--;
            }
        }
    }
    
    for (int i = 0; i < white_eaten_draw; i++) {
        float z_offset = (float)i * 0.65f + 0.0f;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){-1.5f, 0.1f, z_offset});
        bool is_dama = is_piece_dama(game->white_eaten_list[i]);
        draw_piece(gfx, model, black_piece_color, no_highlight, is_dama);
    }
    
    for (int i = 0; i < black_eaten_draw; i++) {
        float z_offset = (float)i * 0.65f + 0.0f;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){8.5f, 0.1f, z_offset});
        bool is_dama = is_piece_dama(game->black_eaten_list[i]);
        draw_piece(gfx, model, white_piece_color, no_highlight, is_dama);
    }
}

