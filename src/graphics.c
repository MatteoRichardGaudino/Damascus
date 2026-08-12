#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
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

void graphics_render_scene(GraphicsContext *gfx, const GameState *game, const Camera *cam, float aspect_ratio, int valid_move_count, const Move *valid_moves, PieceAnim *anim, double current_time) {
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
            if (game->selected_row >= 0 && game->selected_col >= 0) {
                for (int m = 0; m < valid_move_count; m++) {
                    if (valid_moves[m].from_row == game->selected_row &&
                        valid_moves[m].from_col == game->selected_col &&
                        valid_moves[m].to_row == r && valid_moves[m].to_col == c) {
                        highlight[0] = move_target_highlight[0];
                        highlight[1] = move_target_highlight[1];
                        highlight[2] = move_target_highlight[2];
                        highlight[3] = move_target_highlight[3];
                        break;
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
    
    if (anim && anim->active) {
        double elapsed = current_time - anim->start_time;
        double total_duration = anim->move_duration + (anim->has_capture ? anim->capture_duration : 0.0);
        
        if (elapsed >= total_duration) {
            anim->active = false;
        } else {
            // Stage 1: Move jump animation (0.0 to move_duration)
            float move_t = (float)(elapsed / anim->move_duration);
            if (move_t > 1.0f) move_t = 1.0f;
            float smooth_move_t = move_t * move_t * (3.0f - 2.0f * move_t);
            
            is_animating_move = (move_t < 1.0f);
            anim_x = (float)anim->from_col + ((float)anim->to_col - (float)anim->from_col) * smooth_move_t;
            anim_z = (float)anim->from_row + ((float)anim->to_row - (float)anim->from_row) * smooth_move_t;
            anim_y = 0.2f + sinf(smooth_move_t * (float)M_PI) * 0.5f;
            
            // Stage 2: Captured piece arc animation to side border (starts AFTER move_duration)
            if (anim->has_capture && elapsed >= anim->move_duration) {
                double cap_elapsed = elapsed - anim->move_duration;
                float cap_t = (float)(cap_elapsed / anim->capture_duration);
                if (cap_t > 1.0f) cap_t = 1.0f;
                float smooth_cap_t = cap_t * cap_t * (3.0f - 2.0f * cap_t);
                
                is_animating_capture = (cap_t < 1.0f);
                cap_x = (float)anim->captured_col + (anim->captured_target_x - (float)anim->captured_col) * smooth_cap_t;
                cap_z = (float)anim->captured_row + (anim->captured_target_z - (float)anim->captured_row) * smooth_cap_t;
                cap_y = 0.2f + sinf(smooth_cap_t * (float)M_PI) * 1.3f;
            }
        }
    }

    // 3. Draw Pieces on Board
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            // Hide static piece at destination while moving
            if (is_animating_move && r == anim->to_row && c == anim->to_col) {
                continue;
            }
            
            // Hide captured piece on board once Stage 2 starts
            if (anim && anim->active && anim->has_capture && (current_time - anim->start_time) >= anim->move_duration && r == anim->captured_row && c == anim->captured_col) {
                continue;
            }
            
            PieceType p = game->board[r][c];
            // If this tile had a piece captured in the current active animation,
            // keep rendering the captured piece statically on its tile during Stage 1 (before Stage 2 fly-out starts)
            if (p == PIECE_NONE && anim && anim->active && anim->has_capture && r == anim->captured_row && c == anim->captured_col) {
                double elapsed = current_time - anim->start_time;
                if (elapsed < anim->move_duration) {
                    p = anim->captured_type;
                }
            }
            
            if (p == PIECE_NONE) continue;
            
            glm_mat4_identity(model);
            glm_translate(model, (vec3){(float)c, 0.2f, (float)r});
            
            vec4 *p_col = is_piece_white(p) ? &white_piece_color : &black_piece_color;
            bool is_selected = (r == game->selected_row && c == game->selected_col);
            bool is_dama = is_piece_dama(p);
            
            draw_piece(gfx, model, *p_col, is_selected ? select_highlight : no_highlight, is_dama);
        }
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
    if (is_animating_capture && anim) {
        glm_mat4_identity(model);
        glm_translate(model, (vec3){cap_x, cap_y, cap_z});
        vec4 *p_col = is_piece_white(anim->captured_type) ? &white_piece_color : &black_piece_color;
        bool is_dama = is_piece_dama(anim->captured_type);
        draw_piece(gfx, model, *p_col, select_highlight, is_dama);
    }

    // 4. Draw Eaten Pieces on Side Borders (hide the last added piece until capture animation finishes)
    int white_eaten_draw = game->white_eaten_count;
    int black_eaten_draw = game->black_eaten_count;
    
    if (anim && anim->active && anim->has_capture) {
        double elapsed = current_time - anim->start_time;
        if (elapsed < (anim->move_duration + anim->capture_duration)) {
            if (is_piece_black(anim->captured_type) && white_eaten_draw > 0) {
                white_eaten_draw--;
            } else if (is_piece_white(anim->captured_type) && black_eaten_draw > 0) {
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
