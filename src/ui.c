#include "ui.h"
#include "engine.h"
#include <glad/glad.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <string.h>

// 2D Vertex Format: Position (2), Color (4), UV (2), UseTex (1)
typedef struct {
    float x, y;
    float r, g, b, a;
    float u, v;
    float use_tex;
} UIVertex;

#define UI_MAX_VERTS 16384
static UIVertex ui_verts[UI_MAX_VERTS];
static int ui_vert_count = 0;
static GLuint ui_vao = 0;
static GLuint ui_vbo = 0;
static GLuint font_texture = 0;

// Standard 8x8 ASCII font bitmap for characters 32 (' ') to 126 ('~')
static const unsigned char font8x8_basic[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x66,0x66,0xFF,0x66,0xFF,0x66,0x66,0x00}, // '#'
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // '$'
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // '%'
    {0x3C,0x66,0x3C,0x38,0x67,0x66,0x3F,0x00}, // '&'
    {0x06,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '\''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // '.'
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // '/'
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // '1'
    {0x3C,0x66,0x0C,0x18,0x30,0x60,0x7E,0x00}, // '2'
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // '3'
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, // '4'
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // '5'
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, // '6'
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // '8'
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, // '9'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // ':'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // '='
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // '>'
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // '?'
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // '@'
    {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 'D'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // 'E'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'I'
    {0x1E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, // 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 'P'
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // 'Q'
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // '['
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ']'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'a' -> 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 'b' -> 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 'c' -> 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 'd' -> 'D'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // 'e' -> 'E'
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // 'f' -> 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // 'g' -> 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'h' -> 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'i' -> 'I'
    {0x1E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, // 'j' -> 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 'k' -> 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 'l' -> 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 'm' -> 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 'n' -> 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'o' -> 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 'p' -> 'P'
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // 'q' -> 'Q'
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // 'r' -> 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 's' -> 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 't' -> 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'u' -> 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'v' -> 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'w' -> 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 'x' -> 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 'y' -> 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 'z' -> 'Z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // '{'
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // '}'
    {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}  // '~'
};

static void init_font_texture(void) {
    if (font_texture != 0) return;
    if (!glad_glGenTextures) return; // Headless safety guard
    
    // Create 128x128 atlas: 16 columns x 16 rows of 8x8 glyphs
    unsigned char atlas[128 * 128 * 4];
    memset(atlas, 0, sizeof(atlas));
    
    for (int ch = 0; ch < 96; ch++) {
        int col = ch % 16;
        int row = ch / 16;
        
        int base_x = col * 8;
        int base_y = row * 8;
        
        for (int y = 0; y < 8; y++) {
            unsigned char b = font8x8_basic[ch][y];
            for (int x = 0; x < 8; x++) {
                if (b & (1 << (7 - x))) {
                    int px = base_x + x;
                    int py = base_y + y;
                    int idx = (py * 128 + px) * 4;
                    atlas[idx + 0] = 255;
                    atlas[idx + 1] = 255;
                    atlas[idx + 2] = 255;
                    atlas[idx + 3] = 255;
                }
            }
        }
    }
    
    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void ui_begin(void) {
    ui_vert_count = 0;
}

static void ui_add_quad_raw(float x, float y, float w, float h, vec4 color, float u0, float v0, float u1, float v1, float use_tex) {
    if (ui_vert_count + 6 > UI_MAX_VERTS) return;
    
    UIVertex v_tl = { x,     y,     color[0], color[1], color[2], color[3], u0, v0, use_tex };
    UIVertex v_tr = { x + w, y,     color[0], color[1], color[2], color[3], u1, v0, use_tex };
    UIVertex v_br = { x + w, y + h, color[0], color[1], color[2], color[3], u1, v1, use_tex };
    UIVertex v_bl = { x,     y + h, color[0], color[1], color[2], color[3], u0, v1, use_tex };
    
    ui_verts[ui_vert_count++] = v_tl;
    ui_verts[ui_vert_count++] = v_tr;
    ui_verts[ui_vert_count++] = v_br;
    
    ui_verts[ui_vert_count++] = v_tl;
    ui_verts[ui_vert_count++] = v_br;
    ui_verts[ui_vert_count++] = v_bl;
}

static void ui_add_quad(float x, float y, float w, float h, vec4 color) {
    ui_add_quad_raw(x, y, w, h, color, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

static void ui_add_text(const char *str, float x, float y, float scale, vec4 color) {
    int len = (int)strlen(str);
    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;
    
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 32 || c > 126) c = ' ';
        
        int idx = c - 32;
        int col = idx % 16;
        int row = idx / 16;
        
        float u0 = (float)(col * 8) / 128.0f;
        float v0 = (float)(row * 8) / 128.0f;
        float u1 = (float)((col + 1) * 8) / 128.0f;
        float v1 = (float)((row + 1) * 8) / 128.0f;
        
        ui_add_quad_raw(x + (float)i * char_w, y, char_w, char_h, color, u0, v0, u1, v1, 1.0f);
    }
}

static void ui_add_text_centered(const char *str, float cx, float cy, float scale, vec4 color) {
    int len = (int)strlen(str);
    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;
    float total_w = (float)len * char_w;
    
    float start_x = cx - total_w * 0.5f;
    float start_y = cy - char_h * 0.5f;
    
    ui_add_text(str, start_x, start_y, scale, color);
}

static void ui_end_and_draw(GLuint ui_shader, int win_w, int win_h) {
    if (ui_vert_count == 0) return;
    
    if (ui_vao == 0) {
        glGenVertexArrays(1, &ui_vao);
        glGenBuffers(1, &ui_vbo);
        
        glBindVertexArray(ui_vao);
        glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(ui_verts), NULL, GL_DYNAMIC_DRAW);
        
        // Pos (location = 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)0);
        glEnableVertexAttribArray(0);
        // Color (location = 1)
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // UV (location = 2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        // UseTex (location = 3)
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
        
        glBindVertexArray(0);
    }
    
    glUseProgram(ui_shader);
    mat4 proj;
    glm_ortho(0.0f, (float)win_w, (float)win_h, 0.0f, -1.0f, 1.0f, proj);
    
    GLint proj_loc = glGetUniformLocation(ui_shader, "uProjection");
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)proj);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, ui_vert_count * sizeof(UIVertex), ui_verts);
    
    glDrawArrays(GL_TRIANGLES, 0, ui_vert_count);
    
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

static const double s_mcts_time_budgets[] = { 0.20, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00 };
static const float  s_mcts_alphas[]       = { 0.50f, 0.80f, 1.00f, 1.4142f, 1.80f, 2.20f, 2.80f };
static const int    s_mcts_depths[]       = { 20, 35, 50, 70, 100, 150, 200 };
static const float  s_mcts_epsilons[]     = { 0.05f, 0.10f, 0.15f, 0.20f, 0.30f, 0.40f, 1.00f };
static const double s_puct_time_budgets[] = { 0.20, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00 };
static const float  s_puct_c_pucts[]      = { 0.50f, 0.80f, 1.00f, 1.50f, 2.00f, 2.50f, 3.50f };
static const float  s_puct_temperatures[] = { 0.20f, 0.50f, 0.80f, 1.00f, 1.20f, 1.50f, 2.00f };
static const int    s_puct_depths[]       = { 20, 35, 50, 70, 100, 150, 200 };
static const double s_cb_times[]          = { 0.20, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00 };
static const double s_kr_times[]          = { 0.20, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00 };

static void step_double_val(double *val, const double *arr, int count, int delta) {
    int cur_idx = 0;
    double best_diff = 1e9;
    for (int i = 0; i < count; i++) {
        double d = (*val > arr[i]) ? (*val - arr[i]) : (arr[i] - *val);
        if (d < best_diff) { best_diff = d; cur_idx = i; }
    }
    int new_idx = cur_idx + delta;
    if (new_idx < 0) new_idx = 0;
    if (new_idx >= count) new_idx = count - 1;
    *val = arr[new_idx];
}

static void step_float_val(float *val, const float *arr, int count, int delta) {
    int cur_idx = 0;
    float best_diff = 1e9f;
    for (int i = 0; i < count; i++) {
        float d = (*val > arr[i]) ? (*val - arr[i]) : (arr[i] - *val);
        if (d < best_diff) { best_diff = d; cur_idx = i; }
    }
    int new_idx = cur_idx + delta;
    if (new_idx < 0) new_idx = 0;
    if (new_idx >= count) new_idx = count - 1;
    *val = arr[new_idx];
}

static void step_int_val(int *val, const int *arr, int count, int delta) {
    int cur_idx = 0;
    int best_diff = 1000000;
    for (int i = 0; i < count; i++) {
        int d = (*val > arr[i]) ? (*val - arr[i]) : (arr[i] - *val);
        if (d < best_diff) { best_diff = d; cur_idx = i; }
    }
    int new_idx = cur_idx + delta;
    if (new_idx < 0) new_idx = 0;
    if (new_idx >= count) new_idx = count - 1;
    *val = arr[new_idx];
}

void ui_init(UIContext *ui) {
    ui->state = UI_STATE_MAIN_MENU;
    ui->prev_ui_state = UI_STATE_MAIN_MENU;
    ui->settings_tab = 0;
    ui->selected_mode = MODE_2PLAYER;
    ui->selected_human_color = PLAYER_WHITE;
    ui->selected_white_engine = ENGINE_TYPE_RANDOM;
    ui->selected_black_engine = ENGINE_TYPE_RANDOM;
    ui->win_w = 1024;
    ui->win_h = 768;
    
    ui->last_white_ai_time = 0.0;
    ui->last_black_ai_time = 0.0;
    ui->has_white_ai_time = false;
    ui->has_black_ai_time = false;

    engine_config_init_default(&ui->engine_config);
    
    init_font_texture();
}

static inline float ui_get_scale(const UIContext *ui) {
    if (ui->win_w <= 0 || ui->win_h <= 0) return 1.0f;
    float scale_w = (float)ui->win_w / 1024.0f;
    float scale_h = (float)ui->win_h / 768.0f;
    float s = (scale_w < scale_h) ? scale_w : scale_h;
    if (s < 1.0f) s = 1.0f;
    return s;
}

static bool point_in_rect(double px, double py, float rx, float ry, float rw, float rh) {
    return px >= rx && px <= (rx + rw) && py >= ry && py <= (ry + rh);
}

void ui_render(UIContext *ui, GameState *game, GLuint ui_shader) {
    ui_begin();
    
    float S = ui_get_scale(ui);
    
    vec4 backdrop_overlay= { 0.02f, 0.04f, 0.08f, 0.75f }; // Full screen dark dim
    vec4 glass_panel     = { 0.08f, 0.12f, 0.18f, 0.95f }; // Solid dark card
    vec4 border_color    = { 0.35f, 0.55f, 0.85f, 0.80f }; // Vibrant blue glow border
    vec4 btn_normal      = { 0.15f, 0.22f, 0.35f, 0.95f };
    vec4 btn_active      = { 0.22f, 0.50f, 0.90f, 1.00f };
    vec4 btn_start       = { 0.12f, 0.68f, 0.38f, 1.00f };
    vec4 btn_danger      = { 0.75f, 0.20f, 0.20f, 0.95f };
    vec4 btn_stepper     = { 0.18f, 0.28f, 0.44f, 0.95f };
    
    vec4 text_white      = { 1.00f, 1.00f, 1.00f, 1.00f };
    vec4 text_gold       = { 1.00f, 0.82f, 0.20f, 1.00f };
    vec4 text_sub        = { 0.70f, 0.82f, 0.98f, 1.00f };
    vec4 text_green      = { 0.40f, 0.95f, 0.50f, 1.00f };
    vec4 text_red        = { 0.95f, 0.45f, 0.45f, 1.00f };

    if (ui->state == UI_STATE_MAIN_MENU) {
        // 1. Fullscreen Dimmed Backdrop
        ui_add_quad(0.0f, 0.0f, (float)ui->win_w, (float)ui->win_h, backdrop_overlay);
        
        // 2. Main Menu Card (Center of screen)
        float p_w = 560.0f * S;
        float p_h = 515.0f * S;
        float p_x = ((float)ui->win_w - p_w) * 0.5f;
        float p_y = ((float)ui->win_h - p_h) * 0.5f;
        
        // Outer glow & main card panel
        ui_add_quad(p_x - 3.0f * S, p_y - 3.0f * S, p_w + 6.0f * S, p_h + 6.0f * S, border_color);
        ui_add_quad(p_x, p_y, p_w, p_h, glass_panel);
        
        // Title Header
        ui_add_text_centered("DAMASCUS", p_x + p_w * 0.5f, p_y + 32.0f * S, 2.5f * S, text_gold);
        ui_add_text_centered("DAMA ITALIANA 3D", p_x + p_w * 0.5f, p_y + 58.0f * S, 1.3f * S, text_sub);
        
        // Mode Selection Header
        ui_add_text("SELEZIONA MODALITA DI GIOCO:", p_x + 35.0f * S, p_y + 88.0f * S, 1.05f * S, text_white);
        
        // Mode Selection Buttons
        float btn_w = 150.0f * S;
        float btn_h = 38.0f * S;
        float start_x = p_x + 35.0f * S;
        float btn_y = p_y + 108.0f * S;
        
        // 2 Player Button
        vec4 *c1 = (ui->selected_mode == MODE_2PLAYER) ? &btn_active : &btn_normal;
        ui_add_quad(start_x, btn_y, btn_w, btn_h, *c1);
        ui_add_text_centered("2 GIOCATORI", start_x + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.0f * S, text_white);
        
        // 1 Player Button
        vec4 *c2 = (ui->selected_mode == MODE_1PLAYER) ? &btn_active : &btn_normal;
        ui_add_quad(start_x + 165.0f * S, btn_y, btn_w, btn_h, *c2);
        ui_add_text_centered("1 GIOCATORE", start_x + 165.0f * S + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.0f * S, text_white);
        
        // CPU vs CPU Button
        vec4 *c3 = (ui->selected_mode == MODE_CPUVSCPU) ? &btn_active : &btn_normal;
        ui_add_quad(start_x + 330.0f * S, btn_y, btn_w, btn_h, *c3);
        ui_add_text_centered("CPU VS CPU", start_x + 330.0f * S + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.0f * S, text_white);
        
        // Engine / Color Selection Boxes
        float eng_y = p_y + 158.0f * S;
        if (ui->selected_mode == MODE_1PLAYER) {
            ui_add_text("SELEZIONA IL TUO COLORE:", start_x, eng_y, 1.05f * S, text_white);
            
            float col_btn_w = 235.0f * S;
            vec4 *col_w = (ui->selected_human_color == PLAYER_WHITE) ? &btn_active : &btn_normal;
            vec4 *col_b = (ui->selected_human_color == PLAYER_BLACK) ? &btn_active : &btn_normal;
            
            ui_add_quad(start_x, eng_y + 18.0f * S, col_btn_w, 34.0f * S, *col_w);
            ui_add_text_centered("GIOCA COME BIANCO", start_x + col_btn_w * 0.5f, eng_y + 35.0f * S, 1.0f * S, text_white);
            
            ui_add_quad(start_x + 245.0f * S, eng_y + 18.0f * S, col_btn_w, 34.0f * S, *col_b);
            ui_add_text_centered("GIOCA COME NERO", start_x + 245.0f * S + col_btn_w * 0.5f, eng_y + 35.0f * S, 1.0f * S, text_white);
            
            char cpu_str[128];
            snprintf(cpu_str, sizeof(cpu_str), "ENGINE CPU: %s", engine_get_type_name(ui->selected_black_engine));
            ui_add_quad(start_x, eng_y + 58.0f * S, 480.0f * S, 34.0f * S, btn_normal);
            ui_add_text_centered(cpu_str, start_x + 240.0f * S, eng_y + 75.0f * S, 1.05f * S, text_gold);
        } else if (ui->selected_mode == MODE_CPUVSCPU) {
            ui_add_text("SELEZIONA ENGINE CPU:", start_x, eng_y, 1.05f * S, text_white);
            
            char e1_str[64], e2_str[64];
            snprintf(e1_str, sizeof(e1_str), "CPU 1: %s", engine_get_type_name(ui->selected_white_engine));
            snprintf(e2_str, sizeof(e2_str), "CPU 2: %s", engine_get_type_name(ui->selected_black_engine));
            
            ui_add_quad(start_x, eng_y + 18.0f * S, 235.0f * S, 34.0f * S, btn_normal);
            ui_add_text_centered(e1_str, start_x + 117.0f * S, eng_y + 35.0f * S, 1.0f * S, text_gold);
            
            ui_add_quad(start_x + 245.0f * S, eng_y + 18.0f * S, 235.0f * S, 34.0f * S, btn_normal);
            ui_add_text_centered(e2_str, start_x + 245.0f * S + 117.0f * S, eng_y + 35.0f * S, 1.0f * S, text_gold);
        }

        // Thinking Time Profiles (Fast 0.2s | Medium 1.0s | Slow 3.0s)
        float prof_y = p_y + 260.0f * S;
        ui_add_text("PROFILO TEMPO DI RIFLESSIONE (THINKING TIME):", start_x, prof_y, 1.05f * S, text_white);

        TimeProfile cur_prof = engine_config_get_time_profile(&ui->engine_config);
        vec4 *p_fast   = (cur_prof == TIME_PROFILE_FAST)   ? &btn_active : &btn_normal;
        vec4 *p_medium = (cur_prof == TIME_PROFILE_MEDIUM) ? &btn_active : &btn_normal;
        vec4 *p_slow   = (cur_prof == TIME_PROFILE_SLOW)   ? &btn_active : &btn_normal;

        float prof_btn_y = prof_y + 20.0f * S;
        float prof_btn_w = 150.0f * S;
        float prof_btn_h = 35.0f * S;

        ui_add_quad(start_x, prof_btn_y, prof_btn_w, prof_btn_h, *p_fast);
        ui_add_text_centered("FAST (0.2s)", start_x + prof_btn_w * 0.5f, prof_btn_y + prof_btn_h * 0.5f, 1.0f * S, text_white);

        ui_add_quad(start_x + 165.0f * S, prof_btn_y, prof_btn_w, prof_btn_h, *p_medium);
        ui_add_text_centered("MEDIUM (1.0s)", start_x + 165.0f * S + prof_btn_w * 0.5f, prof_btn_y + prof_btn_h * 0.5f, 1.0f * S, text_white);

        ui_add_quad(start_x + 330.0f * S, prof_btn_y, prof_btn_w, prof_btn_h, *p_slow);
        ui_add_text_centered("SLOW (3.0s)", start_x + 330.0f * S + prof_btn_w * 0.5f, prof_btn_y + prof_btn_h * 0.5f, 1.0f * S, text_white);
        
        // Engine detailed settings button
        float cfg_btn_y = p_y + 332.0f * S;
        float cfg_btn_w = 480.0f * S;
        ui_add_quad(start_x, cfg_btn_y, cfg_btn_w, 36.0f * S, btn_stepper);
        ui_add_text_centered("IMPOSTAZIONI DETTAGLIATE MOTORI", start_x + cfg_btn_w * 0.5f, cfg_btn_y + 18.0f * S, 1.05f * S, text_gold);
        
        // Start Game Button
        float start_btn_y = p_y + 385.0f * S;
        float start_btn_w = 340.0f * S;
        float start_btn_h = 52.0f * S;
        float start_btn_x = p_x + (p_w - start_btn_w) * 0.5f;
        
        ui_add_quad(start_btn_x, start_btn_y, start_btn_w, start_btn_h, btn_start);
        ui_add_text_centered("INIZIA PARTITA", start_btn_x + start_btn_w * 0.5f, start_btn_y + start_btn_h * 0.5f, 1.7f * S, text_white);
        
    } else if (ui->state == UI_STATE_ENGINE_SETTINGS) {
        // Fullscreen Dimmed Backdrop
        ui_add_quad(0.0f, 0.0f, (float)ui->win_w, (float)ui->win_h, backdrop_overlay);
        
        // Engine Settings Card
        float s_w = 680.0f * S;
        float s_h = 535.0f * S;
        float s_x = ((float)ui->win_w - s_w) * 0.5f;
        float s_y = ((float)ui->win_h - s_h) * 0.5f;
        
        // Glow border & main panel
        ui_add_quad(s_x - 3.0f * S, s_y - 3.0f * S, s_w + 6.0f * S, s_h + 6.0f * S, border_color);
        ui_add_quad(s_x, s_y, s_w, s_h, glass_panel);
        
        // Header
        ui_add_text_centered("CONFIGURAZIONE PARAMETRI MOTORI", s_x + s_w * 0.5f, s_y + 26.0f * S, 1.5f * S, text_gold);
        
        // Tabs row: MCTS UCB1 | MCTS PUCT | CHECKERBOARD | KINGSROW
        float tab_y = s_y + 50.0f * S;
        float tab_w = 145.0f * S;
        float tab_h = 36.0f * S;
        
        vec4 *t0 = (ui->settings_tab == 0) ? &btn_active : &btn_normal;
        vec4 *t1 = (ui->settings_tab == 1) ? &btn_active : &btn_normal;
        vec4 *t2 = (ui->settings_tab == 2) ? &btn_active : &btn_normal;
        vec4 *t3 = (ui->settings_tab == 3) ? &btn_active : &btn_normal;
        
        ui_add_quad(s_x + 35.0f * S, tab_y, tab_w, tab_h, *t0);
        ui_add_text_centered("MCTS UCB1", s_x + 35.0f * S + tab_w * 0.5f, tab_y + tab_h * 0.5f, 1.0f * S, text_white);
        
        ui_add_quad(s_x + 190.0f * S, tab_y, tab_w, tab_h, *t1);
        ui_add_text_centered("MCTS PUCT", s_x + 190.0f * S + tab_w * 0.5f, tab_y + tab_h * 0.5f, 1.0f * S, text_white);
        
        ui_add_quad(s_x + 345.0f * S, tab_y, tab_w, tab_h, *t2);
        ui_add_text_centered("CHECKERBOARD", s_x + 345.0f * S + tab_w * 0.5f, tab_y + tab_h * 0.5f, 1.0f * S, text_white);
        
        ui_add_quad(s_x + 500.0f * S, tab_y, tab_w, tab_h, *t3);
        ui_add_text_centered("KINGSROW", s_x + 500.0f * S + tab_w * 0.5f, tab_y + tab_h * 0.5f, 1.0f * S, text_white);
        
        // Tab Content
        if (ui->settings_tab == 0) {
            // Tab 0: MCTS UCB1
            float row_x = s_x + 40.0f * S;
            float step_x = s_x + 450.0f * S;
            
            // Parameter 1: Time budget
            float r1_y = s_y + 90.0f * S;
            ui_add_text("TEMPO DI RICERCA (TIME BUDGET):", row_x, r1_y + 8.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r1_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r1_y + 13.0f * S, 1.5f * S, text_white);
            
            char val1[32];
            snprintf(val1, sizeof(val1), "%.2fs", ui->engine_config.mcts_time_budget);
            ui_add_quad(step_x + 45.0f * S, r1_y, 95.0f * S, 28.0f * S, btn_normal);
            ui_add_text_centered(val1, step_x + 92.0f * S, r1_y + 13.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r1_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r1_y + 13.0f * S, 1.5f * S, text_white);
            
            // Parameter 2: Exploration alpha
            float r2_y = s_y + 122.0f * S;
            ui_add_text("PARAMETRO ESPLORAZIONE (ALPHA):", row_x, r2_y + 8.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r2_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r2_y + 13.0f * S, 1.5f * S, text_white);
            
            char val2[32];
            snprintf(val2, sizeof(val2), "%.2f", ui->engine_config.mcts_exploration);
            ui_add_quad(step_x + 45.0f * S, r2_y, 95.0f * S, 28.0f * S, btn_normal);
            ui_add_text_centered(val2, step_x + 92.0f * S, r2_y + 13.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r2_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r2_y + 13.0f * S, 1.5f * S, text_white);
            
            // Parameter 3: Max rollout depth
            float r3_y = s_y + 154.0f * S;
            ui_add_text("MAX PROFONDITA ROLLOUT:", row_x, r3_y + 8.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r3_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r3_y + 13.0f * S, 1.5f * S, text_white);
            
            char val3[32];
            snprintf(val3, sizeof(val3), "%d", ui->engine_config.mcts_max_rollout_depth);
            ui_add_quad(step_x + 45.0f * S, r3_y, 95.0f * S, 28.0f * S, btn_normal);
            ui_add_text_centered(val3, step_x + 92.0f * S, r3_y + 13.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r3_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r3_y + 13.0f * S, 1.5f * S, text_white);
 
            // Parameter 4: Rollout Epsilon (Biased rollout exploration rate)
            float r4_y = s_y + 186.0f * S;
            ui_add_text("EPSILON ROLLOUT (BIAS EURISTICO):", row_x, r4_y + 8.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r4_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r4_y + 13.0f * S, 1.5f * S, text_white);
            
            char val_eps[32];
            if (ui->engine_config.mcts_rollout_epsilon >= 0.99f) {
                snprintf(val_eps, sizeof(val_eps), "1.0(RAND)");
            } else {
                snprintf(val_eps, sizeof(val_eps), "%.2f (%.0f%%)", ui->engine_config.mcts_rollout_epsilon, ui->engine_config.mcts_rollout_epsilon * 100.0f);
            }
            ui_add_quad(step_x + 45.0f * S, r4_y, 95.0f * S, 28.0f * S, btn_normal);
            ui_add_text_centered(val_eps, step_x + 92.0f * S, r4_y + 13.0f * S, 0.95f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r4_y, 40.0f * S, 28.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r4_y + 13.0f * S, 1.5f * S, text_white);
            
            // Parameter 5: Opening Book Mode Selector
            float r5_y = s_y + 218.0f * S;
            ui_add_text("LIBRO APERTURE (OPENING BOOK):", row_x, r5_y + 8.0f * S, 1.05f * S, text_white);
            
            bool book_avail0 = opening_book_is_available(ui->engine_config.book_backend, ui->engine_config.book_custom_path);
            const char *bk_str0 = "DISATTIVATO";
            if (book_avail0) {
                if (ui->engine_config.book_mode == BOOK_MODE_PUCT_GUIDED) bk_str0 = "ALBERO GUIDATO";
                else if (ui->engine_config.book_mode == BOOK_MODE_BEST) bk_str0 = "MIGLIORI (BEST)";
                else if (ui->engine_config.book_mode == BOOK_MODE_GOOD) bk_str0 = "VARIATE (GOOD)";
                else if (ui->engine_config.book_mode == BOOK_MODE_ALL) bk_str0 = "TUTTE LE MOSSE";
            }
            vec4 *bk_btn_c0 = (!book_avail0) ? &btn_danger : 
                              (ui->engine_config.book_mode != BOOK_MODE_OFF ? &btn_start : &btn_normal);
            float bk_btn_w = 225.0f * S;
            ui_add_quad(step_x - 40.0f * S, r5_y, bk_btn_w, 28.0f * S, *bk_btn_c0);
            ui_add_text_centered(bk_str0, step_x - 40.0f * S + bk_btn_w * 0.5f, r5_y + 14.0f * S, 0.90f * S, text_white);
            
            if (!book_avail0) {
                ui_add_text("[FILE .ODB MANCANTE - DISABILITATO]", row_x, r5_y + 28.0f * S, 0.85f * S, text_red);
            } else if (ui->engine_config.book_mode != BOOK_MODE_OFF) {
                ui_add_text("[LIBRO APERTURE: 1.76M POSIZIONI ATTIVO]", row_x, r5_y + 28.0f * S, 0.85f * S, text_green);
            } else {
                ui_add_text("[LIBRO APERTURE: DISATTIVATO]", row_x, r5_y + 28.0f * S, 0.85f * S, text_sub);
            }

            // Parameter 6: Endgame database backend selector
            float r6_y = s_y + 264.0f * S;
            ui_add_text("DATABASE FINALI (WLD TABLEBASE):", row_x, r6_y + 8.0f * S, 1.05f * S, text_white);
            
            WLDStatus st0 = wld_get_status(ui->engine_config.wld_backend);
            vec4 *db_btn_c0 = (ui->engine_config.wld_backend != WLD_BACKEND_NONE && st0.available) ? &btn_start : 
                              (ui->engine_config.wld_backend == WLD_BACKEND_NONE ? &btn_normal : &btn_danger);
            const char *db_str0 = "DISATTIVATO";
            if (ui->engine_config.wld_backend == WLD_BACKEND_OFFICIAL_8PIECE) {
                db_str0 = "UFFICIALE 8P (data/wld)";
            } else if (ui->engine_config.wld_backend == WLD_BACKEND_REDUCED_NATIVE) {
                db_str0 = "RIDOTTO NATIVO (4P)";
            }
            ui_add_quad(step_x - 40.0f * S, r6_y, bk_btn_w, 28.0f * S, *db_btn_c0);
            ui_add_text_centered(db_str0, step_x - 40.0f * S + bk_btn_w * 0.5f, r6_y + 14.0f * S, 0.90f * S, text_white);
            
            // Real-time backend status text
            vec4 *st_col0 = st0.available ? (ui->engine_config.wld_backend == WLD_BACKEND_NONE ? &text_sub : &text_green) : &text_red;
            ui_add_text(st0.status_message, row_x, r6_y + 28.0f * S, 0.85f * S, *st_col0);
 
            // Parameter 7: Debug logging toggle
            float r7_y = s_y + 310.0f * S;
            ui_add_text("LOG DEBUG MOTORE (CONSOLE):", row_x, r7_y + 8.0f * S, 1.05f * S, text_white);
            
            vec4 *log_btn_c = ui->engine_config.mcts_debug_log ? &btn_start : &btn_danger;
            const char *log_str = ui->engine_config.mcts_debug_log ? "ATTIVO (ON)" : "DISATTIVO (OFF)";
            ui_add_quad(step_x, r7_y, 185.0f * S, 28.0f * S, *log_btn_c);
            ui_add_text_centered(log_str, step_x + 92.0f * S, r7_y + 14.0f * S, 1.0f * S, text_white);
            
            // Info text
            ui_add_text("MCTS UCB1 combina esplorazione/sfruttamento e rollout euristici tattici.", row_x, s_y + 368.0f * S, 0.95f * S, text_sub);
            ui_add_text("Il libro di apertura guida l'albero con seed di visite e mosse immediate.", row_x, s_y + 392.0f * S, 0.95f * S, text_sub);
 
        } else if (ui->settings_tab == 1) {
            // Tab 1: MCTS PUCT
            float row_x = s_x + 40.0f * S;
            float step_x = s_x + 450.0f * S;
            
            // Parameter 1: Time budget
            float r1_y = s_y + 86.0f * S;
            ui_add_text("TEMPO DI RICERCA (TIME BUDGET):", row_x, r1_y + 7.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r1_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r1_y + 12.0f * S, 1.5f * S, text_white);
            
            char val1[32];
            snprintf(val1, sizeof(val1), "%.2fs", ui->engine_config.puct_time_budget);
            ui_add_quad(step_x + 45.0f * S, r1_y, 95.0f * S, 26.0f * S, btn_normal);
            ui_add_text_centered(val1, step_x + 92.0f * S, r1_y + 12.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r1_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r1_y + 12.0f * S, 1.5f * S, text_white);
            
            // Parameter 2: Exploration c_puct
            float r2_y = s_y + 114.0f * S;
            ui_add_text("COSTANTE ESPLORAZIONE (c_puct):", row_x, r2_y + 7.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r2_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r2_y + 12.0f * S, 1.5f * S, text_white);
            
            char val2[32];
            snprintf(val2, sizeof(val2), "%.2f", ui->engine_config.puct_c_puct);
            ui_add_quad(step_x + 45.0f * S, r2_y, 95.0f * S, 26.0f * S, btn_normal);
            ui_add_text_centered(val2, step_x + 92.0f * S, r2_y + 12.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r2_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r2_y + 12.0f * S, 1.5f * S, text_white);
 
            // Parameter 3: Temperature (tau)
            float r3_y = s_y + 142.0f * S;
            ui_add_text("TEMPERATURA PRIOR (TAU):", row_x, r3_y + 7.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r3_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r3_y + 12.0f * S, 1.5f * S, text_white);
            
            char val_tau[32];
            snprintf(val_tau, sizeof(val_tau), "%.2f", ui->engine_config.puct_temperature);
            ui_add_quad(step_x + 45.0f * S, r3_y, 95.0f * S, 26.0f * S, btn_normal);
            ui_add_text_centered(val_tau, step_x + 92.0f * S, r3_y + 12.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r3_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r3_y + 12.0f * S, 1.5f * S, text_white);
            
            // Parameter 4: Max rollout depth
            float r4_y = s_y + 170.0f * S;
            ui_add_text("MAX PROFONDITA ROLLOUT:", row_x, r4_y + 7.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r4_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r4_y + 12.0f * S, 1.5f * S, text_white);
            
            char val3[32];
            snprintf(val3, sizeof(val3), "%d", ui->engine_config.puct_max_rollout_depth);
            ui_add_quad(step_x + 45.0f * S, r4_y, 95.0f * S, 26.0f * S, btn_normal);
            ui_add_text_centered(val3, step_x + 92.0f * S, r4_y + 12.0f * S, 1.05f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r4_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r4_y + 12.0f * S, 1.5f * S, text_white);
 
            // Parameter 5: Rollout Epsilon (Biased rollout exploration rate)
            float r5_y = s_y + 198.0f * S;
            ui_add_text("EPSILON ROLLOUT (BIAS EURISTICO):", row_x, r5_y + 7.0f * S, 1.05f * S, text_white);
            ui_add_quad(step_x, r5_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r5_y + 12.0f * S, 1.5f * S, text_white);
            
            char val_puct_eps[32];
            if (ui->engine_config.puct_rollout_epsilon >= 0.99f) {
                snprintf(val_puct_eps, sizeof(val_puct_eps), "1.0(RAND)");
            } else {
                snprintf(val_puct_eps, sizeof(val_puct_eps), "%.2f (%.0f%%)", ui->engine_config.puct_rollout_epsilon, ui->engine_config.puct_rollout_epsilon * 100.0f);
            }
            ui_add_quad(step_x + 45.0f * S, r5_y, 95.0f * S, 26.0f * S, btn_normal);
            ui_add_text_centered(val_puct_eps, step_x + 92.0f * S, r5_y + 12.0f * S, 0.95f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r5_y, 40.0f * S, 26.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r5_y + 12.0f * S, 1.5f * S, text_white);
            
            // Parameter 6: Opening Book Mode Selector
            float r6_y = s_y + 226.0f * S;
            ui_add_text("LIBRO APERTURE (OPENING BOOK):", row_x, r6_y + 7.0f * S, 1.05f * S, text_white);
            
            bool book_avail1 = opening_book_is_available(ui->engine_config.book_backend, ui->engine_config.book_custom_path);
            const char *bk_str1 = "DISATTIVATO";
            if (book_avail1) {
                if (ui->engine_config.book_mode == BOOK_MODE_PUCT_GUIDED) bk_str1 = "PUCT GUIDATO";
                else if (ui->engine_config.book_mode == BOOK_MODE_BEST) bk_str1 = "MIGLIORI (BEST)";
                else if (ui->engine_config.book_mode == BOOK_MODE_GOOD) bk_str1 = "VARIATE (GOOD)";
                else if (ui->engine_config.book_mode == BOOK_MODE_ALL) bk_str1 = "TUTTE LE MOSSE";
            }
            vec4 *bk_btn_c1 = (!book_avail1) ? &btn_danger : 
                              (ui->engine_config.book_mode != BOOK_MODE_OFF ? &btn_start : &btn_normal);
            float bk_btn_w1 = 225.0f * S;
            ui_add_quad(step_x - 40.0f * S, r6_y, bk_btn_w1, 26.0f * S, *bk_btn_c1);
            ui_add_text_centered(bk_str1, step_x - 40.0f * S + bk_btn_w1 * 0.5f, r6_y + 13.0f * S, 0.90f * S, text_white);
            
            if (!book_avail1) {
                ui_add_text("[FILE .ODB MANCANTE - DISABILITATO]", row_x, r6_y + 26.0f * S, 0.85f * S, text_red);
            } else if (ui->engine_config.book_mode != BOOK_MODE_OFF) {
                ui_add_text("[LIBRO APERTURE: 1.76M POSIZIONI ATTIVO]", row_x, r6_y + 26.0f * S, 0.85f * S, text_green);
            } else {
                ui_add_text("[LIBRO APERTURE: DISATTIVATO]", row_x, r6_y + 26.0f * S, 0.85f * S, text_sub);
            }

            // Parameter 7: Endgame database backend selector
            float r7_y = s_y + 268.0f * S;
            ui_add_text("DATABASE FINALI (WLD TABLEBASE):", row_x, r7_y + 7.0f * S, 1.05f * S, text_white);
            
            WLDStatus st1 = wld_get_status(ui->engine_config.wld_backend);
            vec4 *db_btn_c1 = (ui->engine_config.wld_backend != WLD_BACKEND_NONE && st1.available) ? &btn_start : 
                              (ui->engine_config.wld_backend == WLD_BACKEND_NONE ? &btn_normal : &btn_danger);
            const char *db_str1 = "DISATTIVATO";
            if (ui->engine_config.wld_backend == WLD_BACKEND_OFFICIAL_8PIECE) {
                db_str1 = "UFFICIALE 8P (data/wld)";
            } else if (ui->engine_config.wld_backend == WLD_BACKEND_REDUCED_NATIVE) {
                db_str1 = "RIDOTTO NATIVO (4P)";
            }
            ui_add_quad(step_x - 40.0f * S, r7_y, bk_btn_w1, 26.0f * S, *db_btn_c1);
            ui_add_text_centered(db_str1, step_x - 40.0f * S + bk_btn_w1 * 0.5f, r7_y + 13.0f * S, 0.90f * S, text_white);
            
            // Real-time backend status text
            vec4 *st_col1 = st1.available ? (ui->engine_config.wld_backend == WLD_BACKEND_NONE ? &text_sub : &text_green) : &text_red;
            ui_add_text(st1.status_message, row_x, r7_y + 26.0f * S, 0.85f * S, *st_col1);
 
            // Parameter 8: Debug logging toggle
            float r8_y = s_y + 310.0f * S;
            ui_add_text("LOG DEBUG MOTORE (CONSOLE):", row_x, r8_y + 7.0f * S, 1.05f * S, text_white);
            
            vec4 *log_btn_c1 = ui->engine_config.puct_debug_log ? &btn_start : &btn_danger;
            const char *log_str1 = ui->engine_config.puct_debug_log ? "ATTIVO (ON)" : "DISATTIVO (OFF)";
            ui_add_quad(step_x, r8_y, 185.0f * S, 26.0f * S, *log_btn_c1);
            ui_add_text_centered(log_str1, step_x + 92.0f * S, r8_y + 13.0f * S, 1.0f * S, text_white);
            
            // Info text
            ui_add_text("MCTS PUCT integra prior da euristica veloce FID e ricerca AlphaGo-style.", row_x, s_y + 368.0f * S, 0.95f * S, text_sub);
            ui_add_text("Il libro di apertura fonde la distribuzione teorica nei prior PUCT.", row_x, s_y + 392.0f * S, 0.95f * S, text_sub);
 
        } else if (ui->settings_tab == 2) {
            // Tab 2: CHECKERBOARD
            float row_x = s_x + 40.0f * S;
            float step_x = s_x + 450.0f * S;
            
            float r1_y = s_y + 130.0f * S;
            ui_add_text("TEMPO DI RICERCA (SECONDI):", row_x, r1_y + 10.0f * S, 1.1f * S, text_white);
            ui_add_quad(step_x, r1_y, 40.0f * S, 35.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r1_y + 17.0f * S, 1.5f * S, text_white);
            
            char val1[32];
            snprintf(val1, sizeof(val1), "%.2fs", ui->engine_config.cb_search_time);
            ui_add_quad(step_x + 45.0f * S, r1_y, 95.0f * S, 35.0f * S, btn_normal);
            ui_add_text_centered(val1, step_x + 92.0f * S, r1_y + 17.0f * S, 1.1f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r1_y, 40.0f * S, 35.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r1_y + 17.0f * S, 1.5f * S, text_white);
            
            // Info text
            ui_add_text("MOTORE CHECKERBOARD ITALIAN CHECKERS (MARTIN FIERZ)", row_x, s_y + 210.0f * S, 1.1f * S, text_gold);
            ui_add_text("Motore minimax alpha-beta iterativo con potatura e tabella trasposizioni.", row_x, s_y + 245.0f * S, 0.95f * S, text_sub);
            ui_add_text("Valutazione posizionale esperta e rapida convergenza.", row_x, s_y + 270.0f * S, 0.95f * S, text_sub);
            
        } else if (ui->settings_tab == 3) {
            // Tab 3: KINGSROW
            float row_x = s_x + 40.0f * S;
            float step_x = s_x + 450.0f * S;
            
            float r1_y = s_y + 130.0f * S;
            ui_add_text("TEMPO MASSIMO DI RICERCA:", row_x, r1_y + 10.0f * S, 1.1f * S, text_white);
            ui_add_quad(step_x, r1_y, 40.0f * S, 35.0f * S, btn_stepper);
            ui_add_text_centered("-", step_x + 20.0f * S, r1_y + 17.0f * S, 1.5f * S, text_white);
            
            char val1[32];
            snprintf(val1, sizeof(val1), "%.2fs", ui->engine_config.kr_search_time);
            ui_add_quad(step_x + 45.0f * S, r1_y, 95.0f * S, 35.0f * S, btn_normal);
            ui_add_text_centered(val1, step_x + 92.0f * S, r1_y + 17.0f * S, 1.1f * S, text_gold);
            
            ui_add_quad(step_x + 145.0f * S, r1_y, 40.0f * S, 35.0f * S, btn_stepper);
            ui_add_text_centered("+", step_x + 165.0f * S, r1_y + 17.0f * S, 1.5f * S, text_white);
            
            // Info text & status
            ui_add_text("MOTORE KINGSROW ITALIAN CHECKERS (ED GILBERT)", row_x, s_y + 210.0f * S, 1.1f * S, text_gold);
            ui_add_text("Motore campione del mondo con endgame database a 10 pezzi.", row_x, s_y + 240.0f * S, 0.95f * S, text_sub);
            
            if (engine_is_type_available(ENGINE_TYPE_KINGSROW)) {
                ui_add_text("STATO: DISPONIBILE (COLLEGAMENTO IPC BRIDGE ATTIVO)", row_x, s_y + 280.0f * S, 1.0f * S, text_green);
            } else {
                ui_add_text("STATO: NON DISPONIBILE (RICHIEDE WINE SU MACOS/LINUX)", row_x, s_y + 280.0f * S, 1.0f * S, text_red);
            }
        }
        
        // Footer Buttons
        float ftr_y = s_y + 465.0f * S;
        float ftr_btn_w = 280.0f * S;
        float ftr_btn_h = 45.0f * S;
        
        // Reset defaults button
        ui_add_quad(s_x + 40.0f * S, ftr_y, ftr_btn_w, ftr_btn_h, btn_normal);
        ui_add_text_centered("RIPRISTINA PREDEFINITI", s_x + 40.0f * S + ftr_btn_w * 0.5f, ftr_y + 22.0f * S, 1.1f * S, text_white);
        
        // Save and Close button
        ui_add_quad(s_x + 360.0f * S, ftr_y, ftr_btn_w, ftr_btn_h, btn_start);
        ui_add_text_centered("SALVA E CHIUDI", s_x + 360.0f * S + ftr_btn_w * 0.5f, ftr_y + 22.0f * S, 1.2f * S, text_white);
        
    } else if (ui->state == UI_STATE_PLAYING) {
        // Top Left Turn Box with solid dark background panel
        ui_add_quad(18.0f * S, 18.0f * S, 254.0f * S, 54.0f * S, border_color);
        ui_add_quad(20.0f * S, 20.0f * S, 250.0f * S, 50.0f * S, glass_panel);
        
        const char *turn_str = (game->current_player == PLAYER_WHITE) ? "TURNO: BIANCO" : "TURNO: NERO";
        ui_add_text_centered(turn_str, 145.0f * S, 45.0f * S, 1.4f * S, text_gold);
        
        // White Player / AI Response Time Badge
        float w_lbl_x = 285.0f * S;
        float lbl_w = 210.0f * S;
        float lbl_h = 50.0f * S;
        ui_add_quad(w_lbl_x - 2.0f * S, 18.0f * S, lbl_w + 4.0f * S, lbl_h + 4.0f * S, border_color);
        ui_add_quad(w_lbl_x, 20.0f * S, lbl_w, lbl_h, glass_panel);

        bool white_is_ai = (game->mode == MODE_CPUVSCPU) || (game->mode == MODE_1PLAYER && game->human_player != PLAYER_WHITE);
        if (white_is_ai) {
            char title_str[64];
            snprintf(title_str, sizeof(title_str), "BIANCO (%s)", engine_get_type_name(game->white_engine));
            char val_str[64];
            if (ui->has_white_ai_time) {
                snprintf(val_str, sizeof(val_str), "TEMPO: %.2fs", ui->last_white_ai_time);
            } else {
                snprintf(val_str, sizeof(val_str), "TEMPO: --");
            }
            ui_add_text_centered(title_str, w_lbl_x + lbl_w * 0.5f, 34.0f * S, 0.95f * S, text_sub);
            ui_add_text_centered(val_str, w_lbl_x + lbl_w * 0.5f, 54.0f * S, 1.1f * S, text_gold);
        } else {
            ui_add_text_centered("BIANCO", w_lbl_x + lbl_w * 0.5f, 34.0f * S, 0.95f * S, text_sub);
            ui_add_text_centered("UMANO", w_lbl_x + lbl_w * 0.5f, 54.0f * S, 1.1f * S, text_white);
        }

        // Black Player / AI Response Time Badge
        float b_lbl_x = 510.0f * S;
        ui_add_quad(b_lbl_x - 2.0f * S, 18.0f * S, lbl_w + 4.0f * S, lbl_h + 4.0f * S, border_color);
        ui_add_quad(b_lbl_x, 20.0f * S, lbl_w, lbl_h, glass_panel);

        bool black_is_ai = (game->mode == MODE_CPUVSCPU) || (game->mode == MODE_1PLAYER && game->human_player != PLAYER_BLACK);
        if (black_is_ai) {
            char title_str[64];
            snprintf(title_str, sizeof(title_str), "NERO (%s)", engine_get_type_name(game->black_engine));
            char val_str[64];
            if (ui->has_black_ai_time) {
                snprintf(val_str, sizeof(val_str), "TEMPO: %.2fs", ui->last_black_ai_time);
            } else {
                snprintf(val_str, sizeof(val_str), "TEMPO: --");
            }
            ui_add_text_centered(title_str, b_lbl_x + lbl_w * 0.5f, 34.0f * S, 0.95f * S, text_sub);
            ui_add_text_centered(val_str, b_lbl_x + lbl_w * 0.5f, 54.0f * S, 1.1f * S, text_gold);
        } else {
            ui_add_text_centered("NERO", b_lbl_x + lbl_w * 0.5f, 34.0f * S, 0.95f * S, text_sub);
            ui_add_text_centered("UMANO", b_lbl_x + lbl_w * 0.5f, 54.0f * S, 1.1f * S, text_white);
        }
        
        // Top Right Menu Button & Engine Settings Button
        float menu_btn_x = (float)ui->win_w - 275.0f * S;
        ui_add_quad(menu_btn_x - 2.0f * S, 18.0f * S, 124.0f * S, 49.0f * S, border_color);
        ui_add_quad(menu_btn_x, 20.0f * S, 120.0f * S, 45.0f * S, btn_normal);
        ui_add_text_centered("MENU", menu_btn_x + 60.0f * S, 42.0f * S, 1.2f * S, text_white);

        float cfg_top_x = (float)ui->win_w - 140.0f * S;
        ui_add_quad(cfg_top_x - 2.0f * S, 18.0f * S, 124.0f * S, 49.0f * S, border_color);
        ui_add_quad(cfg_top_x, 20.0f * S, 120.0f * S, 45.0f * S, btn_stepper);
        ui_add_text_centered("MOTORI", cfg_top_x + 60.0f * S, 42.0f * S, 1.2f * S, text_gold);
        
        // Repetition Warning Indicator (Visible only if current position has been repeated 2 times)
        if (!game->is_game_over) {
            int rep_count = game_get_repetition_count(game);
            if (rep_count == 2) {
                float rep_w = 340.0f * S;
                float rep_h = 46.0f * S;
                float rep_x = 20.0f * S;
                float rep_y = 78.0f * S;

                vec4 warn_border = { 0.95f, 0.60f, 0.10f, 0.90f }; // Vivid Amber Border
                vec4 warn_glass  = { 0.22f, 0.14f, 0.05f, 0.90f }; // Warm Dark Amber Glass
                vec4 text_warn   = { 1.00f, 0.75f, 0.20f, 1.00f }; // Bright Gold

                ui_add_quad(rep_x - 2.0f * S, rep_y - 2.0f * S, rep_w + 4.0f * S, rep_h + 4.0f * S, warn_border);
                ui_add_quad(rep_x, rep_y, rep_w, rep_h, warn_glass);
                ui_add_text_centered("AVVISO PATTA (RIPETIZIONE 2/3)", rep_x + rep_w * 0.5f, rep_y + 16.0f * S, 0.95f * S, text_warn);
                ui_add_text_centered("1 RIPETIZIONE RIMASTA ALLA PATTA", rep_x + rep_w * 0.5f, rep_y + 32.0f * S, 0.90f * S, text_white);
            }
        }
        
        // Game Over Overlay Modal
        if (game->is_game_over) {
            ui_add_quad(0.0f, 0.0f, (float)ui->win_w, (float)ui->win_h, backdrop_overlay);
            
            float o_w = 420.0f * S;
            float o_h = 220.0f * S;
            float o_x = ((float)ui->win_w - o_w) * 0.5f;
            float o_y = ((float)ui->win_h - o_h) * 0.5f;
            
            ui_add_quad(o_x - 3.0f * S, o_y - 3.0f * S, o_w + 6.0f * S, o_h + 6.0f * S, border_color);
            ui_add_quad(o_x, o_y, o_w, o_h, glass_panel);
            
            ui_add_text_centered("PARTITA FINITA!", o_x + o_w * 0.5f, o_y + 40.0f * S, 2.0f * S, text_gold);
            
            if (game->is_draw) {
                ui_add_text_centered("PATTA PER RIPETIZIONE (3/3)", o_x + o_w * 0.5f, o_y + 85.0f * S, 1.3f * S, text_gold);
            } else {
                const char *win_msg = (game->winner == PLAYER_WHITE) ? "HA VINTO IL BIANCO!" : "HA VINTO IL NERO!";
                ui_add_text_centered(win_msg, o_x + o_w * 0.5f, o_y + 85.0f * S, 1.5f * S, text_white);
            }
            
            // Restart Button
            float r_w = 240.0f * S;
            float r_h = 50.0f * S;
            float r_x = o_x + (o_w - r_w) * 0.5f;
            float r_y = o_y + 135.0f * S;
            
            ui_add_quad(r_x, r_y, r_w, r_h, btn_start);
            ui_add_text_centered("NUOVA PARTITA", r_x + r_w * 0.5f, r_y + r_h * 0.5f, 1.4f * S, text_white);
        }

    }
    
    ui_end_and_draw(ui_shader, ui->win_w, ui->win_h);
}

bool ui_handle_click(UIContext *ui, GameState *game, double mouse_x, double mouse_y) {
    float S = ui_get_scale(ui);

    if (ui->state == UI_STATE_MAIN_MENU) {
        float p_w = 560.0f * S;
        float p_h = 515.0f * S;
        float p_x = ((float)ui->win_w - p_w) * 0.5f;
        float p_y = ((float)ui->win_h - p_h) * 0.5f;
        
        float btn_w = 150.0f * S;
        float btn_h = 38.0f * S;
        float start_x = p_x + 35.0f * S;
        float btn_y = p_y + 108.0f * S;
        
        // Mode 2Player
        if (point_in_rect(mouse_x, mouse_y, start_x, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_2PLAYER;
            return true;
        }
        // Mode 1Player
        if (point_in_rect(mouse_x, mouse_y, start_x + 165.0f * S, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_1PLAYER;
            return true;
        }
        // Mode CPU vs CPU
        if (point_in_rect(mouse_x, mouse_y, start_x + 330.0f * S, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_CPUVSCPU;
            return true;
        }
        
        // Color selection for 1Player mode
        if (ui->selected_mode == MODE_1PLAYER) {
            float eng_y = p_y + 158.0f * S;
            float col_btn_w = 235.0f * S;
            
            // White button
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 18.0f * S, col_btn_w, 34.0f * S)) {
                ui->selected_human_color = PLAYER_WHITE;
                return true;
            }
            // Black button
            if (point_in_rect(mouse_x, mouse_y, start_x + 245.0f * S, eng_y + 18.0f * S, col_btn_w, 34.0f * S)) {
                ui->selected_human_color = PLAYER_BLACK;
                return true;
            }

            // CPU Engine Selection button in 1Player mode
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 58.0f * S, 480.0f * S, 34.0f * S)) {
                do {
                    ui->selected_black_engine = (ui->selected_black_engine + 1) % ENGINE_TYPE_COUNT;
                } while (!engine_is_type_available(ui->selected_black_engine));
                return true;
            }
        } else if (ui->selected_mode == MODE_CPUVSCPU) {
            float eng_y = p_y + 158.0f * S;
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 18.0f * S, 235.0f * S, 34.0f * S)) {
                do {
                    ui->selected_white_engine = (ui->selected_white_engine + 1) % ENGINE_TYPE_COUNT;
                } while (!engine_is_type_available(ui->selected_white_engine));
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, start_x + 245.0f * S, eng_y + 18.0f * S, 235.0f * S, 34.0f * S)) {
                do {
                    ui->selected_black_engine = (ui->selected_black_engine + 1) % ENGINE_TYPE_COUNT;
                } while (!engine_is_type_available(ui->selected_black_engine));
                return true;
            }
        }

        // Thinking Time Profiles Selection (Fast 0.2s | Medium 1.0s | Slow 3.0s)
        float prof_btn_y = p_y + 280.0f * S;
        float prof_btn_w = 150.0f * S;
        float prof_btn_h = 35.0f * S;
        if (point_in_rect(mouse_x, mouse_y, start_x, prof_btn_y, prof_btn_w, prof_btn_h)) {
            engine_config_set_time_profile(&ui->engine_config, TIME_PROFILE_FAST);
            return true;
        }
        if (point_in_rect(mouse_x, mouse_y, start_x + 165.0f * S, prof_btn_y, prof_btn_w, prof_btn_h)) {
            engine_config_set_time_profile(&ui->engine_config, TIME_PROFILE_MEDIUM);
            return true;
        }
        if (point_in_rect(mouse_x, mouse_y, start_x + 330.0f * S, prof_btn_y, prof_btn_w, prof_btn_h)) {
            engine_config_set_time_profile(&ui->engine_config, TIME_PROFILE_SLOW);
            return true;
        }
        
        // Open Engine Settings Button
        float cfg_btn_y = p_y + 332.0f * S;
        float cfg_btn_w = 480.0f * S;
        if (point_in_rect(mouse_x, mouse_y, start_x, cfg_btn_y, cfg_btn_w, 36.0f * S)) {
            ui->prev_ui_state = ui->state;
            ui->state = UI_STATE_ENGINE_SETTINGS;
            return true;
        }

        // Start Game Button
        float start_btn_y = p_y + 385.0f * S;
        float start_btn_w = 340.0f * S;
        float start_btn_h = 52.0f * S;
        float start_btn_x = p_x + (p_w - start_btn_w) * 0.5f;
        
        if (point_in_rect(mouse_x, mouse_y, start_btn_x, start_btn_y, start_btn_w, start_btn_h)) {
            ui->state = UI_STATE_PLAYING;
            ui->has_white_ai_time = false;
            ui->has_black_ai_time = false;
            ui->last_white_ai_time = 0.0;
            ui->last_black_ai_time = 0.0;
            game_init(game, ui->selected_mode, ui->selected_human_color, ui->selected_white_engine, ui->selected_black_engine);
            return true;
        }
    } else if (ui->state == UI_STATE_ENGINE_SETTINGS) {
        float s_w = 680.0f * S;
        float s_h = 535.0f * S;
        float s_x = ((float)ui->win_w - s_w) * 0.5f;
        float s_y = ((float)ui->win_h - s_h) * 0.5f;
        
        // Tab switching: 4 tabs (UCB1, PUCT, CHECKERBOARD, KINGSROW)
        float tab_y = s_y + 50.0f * S;
        float tab_w = 145.0f * S;
        float tab_h = 36.0f * S;
        
        if (point_in_rect(mouse_x, mouse_y, s_x + 35.0f * S, tab_y, tab_w, tab_h)) {
            ui->settings_tab = 0;
            return true;
        }
        if (point_in_rect(mouse_x, mouse_y, s_x + 190.0f * S, tab_y, tab_w, tab_h)) {
            ui->settings_tab = 1;
            return true;
        }
        if (point_in_rect(mouse_x, mouse_y, s_x + 345.0f * S, tab_y, tab_w, tab_h)) {
            ui->settings_tab = 2;
            return true;
        }
        if (point_in_rect(mouse_x, mouse_y, s_x + 500.0f * S, tab_y, tab_w, tab_h)) {
            ui->settings_tab = 3;
            return true;
        }
        
        // Controls per Tab
        float step_x = s_x + 450.0f * S;
        if (ui->settings_tab == 0) {
            // MCTS UCB1 Controls
            float r1_y = s_y + 90.0f * S;
            float r2_y = s_y + 122.0f * S;
            float r3_y = s_y + 154.0f * S;
            float r4_y = s_y + 186.0f * S;
            float r5_y = s_y + 218.0f * S;
            float r6_y = s_y + 264.0f * S;
            float r7_y = s_y + 310.0f * S;
            
            // Parameter 1: Time budget (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r1_y, 40.0f * S, 28.0f * S)) {
                step_double_val(&ui->engine_config.mcts_time_budget, s_mcts_time_budgets, sizeof(s_mcts_time_budgets)/sizeof(double), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r1_y, 40.0f * S, 28.0f * S)) {
                step_double_val(&ui->engine_config.mcts_time_budget, s_mcts_time_budgets, sizeof(s_mcts_time_budgets)/sizeof(double), 1);
                return true;
            }
            
            // Parameter 2: Alpha (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r2_y, 40.0f * S, 28.0f * S)) {
                step_float_val(&ui->engine_config.mcts_exploration, s_mcts_alphas, sizeof(s_mcts_alphas)/sizeof(float), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r2_y, 40.0f * S, 28.0f * S)) {
                step_float_val(&ui->engine_config.mcts_exploration, s_mcts_alphas, sizeof(s_mcts_alphas)/sizeof(float), 1);
                return true;
            }
            
            // Parameter 3: Max rollout depth (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r3_y, 40.0f * S, 28.0f * S)) {
                step_int_val(&ui->engine_config.mcts_max_rollout_depth, s_mcts_depths, sizeof(s_mcts_depths)/sizeof(int), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r3_y, 40.0f * S, 28.0f * S)) {
                step_int_val(&ui->engine_config.mcts_max_rollout_depth, s_mcts_depths, sizeof(s_mcts_depths)/sizeof(int), 1);
                return true;
            }

            // Parameter 4: Rollout Epsilon (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r4_y, 40.0f * S, 28.0f * S)) {
                step_float_val(&ui->engine_config.mcts_rollout_epsilon, s_mcts_epsilons, sizeof(s_mcts_epsilons)/sizeof(float), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r4_y, 40.0f * S, 28.0f * S)) {
                step_float_val(&ui->engine_config.mcts_rollout_epsilon, s_mcts_epsilons, sizeof(s_mcts_epsilons)/sizeof(float), 1);
                return true;
            }
            
            // Parameter 5: Opening book mode selector
            float bk_btn_w = 225.0f * S;
            if (point_in_rect(mouse_x, mouse_y, step_x - 40.0f * S, r5_y, bk_btn_w, 28.0f * S)) {
                bool avail = opening_book_is_available(ui->engine_config.book_backend, ui->engine_config.book_custom_path);
                if (!avail) {
                    ui->engine_config.book_mode = BOOK_MODE_OFF;
                    ui->engine_config.mcts_use_book = false;
                    ui->engine_config.puct_use_book = false;
                } else {
                    BookPlayMode next_m = (ui->engine_config.book_mode == BOOK_MODE_PUCT_GUIDED) ? BOOK_MODE_BEST :
                                          (ui->engine_config.book_mode == BOOK_MODE_BEST) ? BOOK_MODE_GOOD :
                                          (ui->engine_config.book_mode == BOOK_MODE_GOOD) ? BOOK_MODE_OFF : BOOK_MODE_PUCT_GUIDED;
                    ui->engine_config.book_mode = next_m;
                    ui->engine_config.mcts_use_book = (next_m != BOOK_MODE_OFF);
                    ui->engine_config.puct_use_book = (next_m != BOOK_MODE_OFF);
                }
                return true;
            }

            // Parameter 6: Database backend selector
            if (point_in_rect(mouse_x, mouse_y, step_x - 40.0f * S, r6_y, bk_btn_w, 28.0f * S)) {
                WLDBackendType next_b = (ui->engine_config.wld_backend == WLD_BACKEND_OFFICIAL_8PIECE) ? WLD_BACKEND_REDUCED_NATIVE :
                                        (ui->engine_config.wld_backend == WLD_BACKEND_REDUCED_NATIVE ? WLD_BACKEND_NONE : WLD_BACKEND_OFFICIAL_8PIECE);
                ui->engine_config.wld_backend = next_b;
                ui->engine_config.mcts_use_db = (next_b != WLD_BACKEND_NONE);
                ui->engine_config.puct_use_db = (next_b != WLD_BACKEND_NONE);
                return true;
            }

            // Parameter 7: Debug log toggle
            if (point_in_rect(mouse_x, mouse_y, step_x, r7_y, 185.0f * S, 28.0f * S)) {
                ui->engine_config.mcts_debug_log = !ui->engine_config.mcts_debug_log;
                return true;
            }
            
        } else if (ui->settings_tab == 1) {
            // MCTS PUCT Controls
            float r1_y = s_y + 86.0f * S;
            float r2_y = s_y + 114.0f * S;
            float r3_y = s_y + 142.0f * S;
            float r4_y = s_y + 170.0f * S;
            float r5_y = s_y + 198.0f * S;
            float r6_y = s_y + 226.0f * S;
            float r7_y = s_y + 268.0f * S;
            float r8_y = s_y + 310.0f * S;
            
            // Parameter 1: Time budget (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r1_y, 40.0f * S, 26.0f * S)) {
                step_double_val(&ui->engine_config.puct_time_budget, s_puct_time_budgets, sizeof(s_puct_time_budgets)/sizeof(double), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r1_y, 40.0f * S, 26.0f * S)) {
                step_double_val(&ui->engine_config.puct_time_budget, s_puct_time_budgets, sizeof(s_puct_time_budgets)/sizeof(double), 1);
                return true;
            }
            
            // Parameter 2: c_puct (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r2_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_c_puct, s_puct_c_pucts, sizeof(s_puct_c_pucts)/sizeof(float), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r2_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_c_puct, s_puct_c_pucts, sizeof(s_puct_c_pucts)/sizeof(float), 1);
                return true;
            }

            // Parameter 3: Temperature (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r3_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_temperature, s_puct_temperatures, sizeof(s_puct_temperatures)/sizeof(float), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r3_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_temperature, s_puct_temperatures, sizeof(s_puct_temperatures)/sizeof(float), 1);
                return true;
            }
            
            // Parameter 4: Max rollout depth (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r4_y, 40.0f * S, 26.0f * S)) {
                step_int_val(&ui->engine_config.puct_max_rollout_depth, s_puct_depths, sizeof(s_puct_depths)/sizeof(int), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r4_y, 40.0f * S, 26.0f * S)) {
                step_int_val(&ui->engine_config.puct_max_rollout_depth, s_puct_depths, sizeof(s_puct_depths)/sizeof(int), 1);
                return true;
            }

            // Parameter 5: Rollout Epsilon (- / +)
            if (point_in_rect(mouse_x, mouse_y, step_x, r5_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_rollout_epsilon, s_mcts_epsilons, sizeof(s_mcts_epsilons)/sizeof(float), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r5_y, 40.0f * S, 26.0f * S)) {
                step_float_val(&ui->engine_config.puct_rollout_epsilon, s_mcts_epsilons, sizeof(s_mcts_epsilons)/sizeof(float), 1);
                return true;
            }
            
            // Parameter 6: Opening book mode selector
            float bk_btn_w1 = 225.0f * S;
            if (point_in_rect(mouse_x, mouse_y, step_x - 40.0f * S, r6_y, bk_btn_w1, 26.0f * S)) {
                bool avail = opening_book_is_available(ui->engine_config.book_backend, ui->engine_config.book_custom_path);
                if (!avail) {
                    ui->engine_config.book_mode = BOOK_MODE_OFF;
                    ui->engine_config.mcts_use_book = false;
                    ui->engine_config.puct_use_book = false;
                } else {
                    BookPlayMode next_m = (ui->engine_config.book_mode == BOOK_MODE_PUCT_GUIDED) ? BOOK_MODE_BEST :
                                          (ui->engine_config.book_mode == BOOK_MODE_BEST) ? BOOK_MODE_GOOD :
                                          (ui->engine_config.book_mode == BOOK_MODE_GOOD) ? BOOK_MODE_OFF : BOOK_MODE_PUCT_GUIDED;
                    ui->engine_config.book_mode = next_m;
                    ui->engine_config.mcts_use_book = (next_m != BOOK_MODE_OFF);
                    ui->engine_config.puct_use_book = (next_m != BOOK_MODE_OFF);
                }
                return true;
            }

            // Parameter 7: Database backend selector
            if (point_in_rect(mouse_x, mouse_y, step_x - 40.0f * S, r7_y, bk_btn_w1, 26.0f * S)) {
                WLDBackendType next_b = (ui->engine_config.wld_backend == WLD_BACKEND_OFFICIAL_8PIECE) ? WLD_BACKEND_REDUCED_NATIVE :
                                        (ui->engine_config.wld_backend == WLD_BACKEND_REDUCED_NATIVE ? WLD_BACKEND_NONE : WLD_BACKEND_OFFICIAL_8PIECE);
                ui->engine_config.wld_backend = next_b;
                ui->engine_config.mcts_use_db = (next_b != WLD_BACKEND_NONE);
                ui->engine_config.puct_use_db = (next_b != WLD_BACKEND_NONE);
                return true;
            }

            // Parameter 8: Debug log toggle
            if (point_in_rect(mouse_x, mouse_y, step_x, r8_y, 185.0f * S, 26.0f * S)) {
                ui->engine_config.puct_debug_log = !ui->engine_config.puct_debug_log;
                return true;
            }
            
        } else if (ui->settings_tab == 2) {
            // CheckerBoard Controls
            float r1_y = s_y + 130.0f * S;
            if (point_in_rect(mouse_x, mouse_y, step_x, r1_y, 40.0f * S, 35.0f * S)) {
                step_double_val(&ui->engine_config.cb_search_time, s_cb_times, sizeof(s_cb_times)/sizeof(double), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r1_y, 40.0f * S, 35.0f * S)) {
                step_double_val(&ui->engine_config.cb_search_time, s_cb_times, sizeof(s_cb_times)/sizeof(double), 1);
                return true;
            }
        } else if (ui->settings_tab == 3) {
            // Kingsrow Controls
            float r1_y = s_y + 130.0f * S;
            if (point_in_rect(mouse_x, mouse_y, step_x, r1_y, 40.0f * S, 35.0f * S)) {
                step_double_val(&ui->engine_config.kr_search_time, s_kr_times, sizeof(s_kr_times)/sizeof(double), -1);
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, step_x + 145.0f * S, r1_y, 40.0f * S, 35.0f * S)) {
                step_double_val(&ui->engine_config.kr_search_time, s_kr_times, sizeof(s_kr_times)/sizeof(double), 1);
                return true;
            }
        }
        
        // Footer actions
        float ftr_y = s_y + 465.0f * S;
        float ftr_btn_w = 280.0f * S;
        float ftr_btn_h = 45.0f * S;
        
        // Reset defaults
        if (point_in_rect(mouse_x, mouse_y, s_x + 40.0f * S, ftr_y, ftr_btn_w, ftr_btn_h)) {
            engine_config_init_default(&ui->engine_config);
            return true;
        }
        
        // Save and Close
        if (point_in_rect(mouse_x, mouse_y, s_x + 360.0f * S, ftr_y, ftr_btn_w, ftr_btn_h)) {
            ui->state = ui->prev_ui_state;
            return true;
        }
        
    } else if (ui->state == UI_STATE_PLAYING) {
        // Back to Main Menu Button
        float menu_btn_x = (float)ui->win_w - 275.0f * S;
        if (point_in_rect(mouse_x, mouse_y, menu_btn_x, 20.0f * S, 120.0f * S, 45.0f * S)) {
            ui->state = UI_STATE_MAIN_MENU;
            return true;
        }
        
        // Open Engine Settings Button from Playing HUD
        float cfg_top_x = (float)ui->win_w - 140.0f * S;
        if (point_in_rect(mouse_x, mouse_y, cfg_top_x, 20.0f * S, 120.0f * S, 45.0f * S)) {
            ui->prev_ui_state = ui->state;
            ui->state = UI_STATE_ENGINE_SETTINGS;
            return true;
        }
        
        if (game->is_game_over) {
            float o_w = 420.0f * S;
            float o_h = 220.0f * S;
            float o_x = ((float)ui->win_w - o_w) * 0.5f;
            float o_y = ((float)ui->win_h - o_h) * 0.5f;
            
            float r_w = 240.0f * S;
            float r_h = 50.0f * S;
            float r_x = o_x + (o_w - r_w) * 0.5f;
            float r_y = o_y + 135.0f * S;
            
            if (point_in_rect(mouse_x, mouse_y, r_x, r_y, r_w, r_h)) {
                ui->has_white_ai_time = false;
                ui->has_black_ai_time = false;
                ui->last_white_ai_time = 0.0;
                ui->last_black_ai_time = 0.0;
                game_init(game, ui->selected_mode, ui->selected_human_color, ui->selected_white_engine, ui->selected_black_engine);
                return true;
            }
        }
    }
    
    return false;
}

