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

static UIVertex ui_verts[4096];
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
    if (ui_vert_count + 6 > 4096) return;
    
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

void ui_init(UIContext *ui) {
    ui->state = UI_STATE_MAIN_MENU;
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
    
    init_font_texture();
}

static bool point_in_rect(double px, double py, float rx, float ry, float rw, float rh) {
    return px >= rx && px <= (rx + rw) && py >= ry && py <= (ry + rh);
}

void ui_render(UIContext *ui, GameState *game, GLuint ui_shader) {
    ui_begin();
    
    vec4 backdrop_overlay= { 0.02f, 0.04f, 0.08f, 0.70f }; // Full screen dark dim
    vec4 glass_panel     = { 0.08f, 0.12f, 0.18f, 0.95f }; // Solid dark card
    vec4 border_color    = { 0.35f, 0.55f, 0.85f, 0.80f }; // Vibrant blue glow border
    vec4 btn_normal      = { 0.15f, 0.22f, 0.35f, 0.95f };
    vec4 btn_active      = { 0.22f, 0.50f, 0.90f, 1.00f };
    vec4 btn_start       = { 0.12f, 0.68f, 0.38f, 1.00f };
    
    vec4 text_white      = { 1.00f, 1.00f, 1.00f, 1.00f };
    vec4 text_gold       = { 1.00f, 0.82f, 0.20f, 1.00f };
    vec4 text_sub        = { 0.70f, 0.82f, 0.98f, 1.00f };

    if (ui->state == UI_STATE_MAIN_MENU) {
        // 1. Fullscreen Dimmed Backdrop (makes 3D background subtly visible while ensuring UI text is 100% readable)
        ui_add_quad(0.0f, 0.0f, (float)ui->win_w, (float)ui->win_h, backdrop_overlay);
        
        // 2. Main Menu Card (Center of screen)
        float p_w = 540.0f;
        float p_h = 440.0f;
        float p_x = ((float)ui->win_w - p_w) * 0.5f;
        float p_y = ((float)ui->win_h - p_h) * 0.5f;
        
        // Outer glow & main card panel
        ui_add_quad(p_x - 3.0f, p_y - 3.0f, p_w + 6.0f, p_h + 6.0f, border_color);
        ui_add_quad(p_x, p_y, p_w, p_h, glass_panel);
        
        // Title Header
        ui_add_text_centered("DAMASCUS", p_x + p_w * 0.5f, p_y + 38.0f, 2.5f, text_gold);
        ui_add_text_centered("DAMA ITALIANA 3D", p_x + p_w * 0.5f, p_y + 68.0f, 1.3f, text_sub);
        
        // Mode Selection Header
        ui_add_text("SELEZIONA MODALITA DI GIOCO:", p_x + 35.0f, p_y + 105.0f, 1.2f, text_white);
        
        // Mode Selection Buttons
        float btn_w = 145.0f;
        float btn_h = 45.0f;
        float start_x = p_x + 35.0f;
        float btn_y = p_y + 130.0f;
        
        // 2 Player Button
        vec4 *c1 = (ui->selected_mode == MODE_2PLAYER) ? &btn_active : &btn_normal;
        ui_add_quad(start_x, btn_y, btn_w, btn_h, *c1);
        ui_add_text_centered("2 GIOCATORI", start_x + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.1f, text_white);
        
        // 1 Player Button
        vec4 *c2 = (ui->selected_mode == MODE_1PLAYER) ? &btn_active : &btn_normal;
        ui_add_quad(start_x + 160.0f, btn_y, btn_w, btn_h, *c2);
        ui_add_text_centered("1 GIOCATORE", start_x + 160.0f + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.1f, text_white);
        
        // CPU vs CPU Button
        vec4 *c3 = (ui->selected_mode == MODE_CPUVSCPU) ? &btn_active : &btn_normal;
        ui_add_quad(start_x + 320.0f, btn_y, btn_w, btn_h, *c3);
        ui_add_text_centered("CPU VS CPU", start_x + 320.0f + btn_w * 0.5f, btn_y + btn_h * 0.5f, 1.1f, text_white);
        
        // Engine / Color Selection Boxes
        float eng_y = p_y + 195.0f;
        if (ui->selected_mode == MODE_1PLAYER) {
            ui_add_text("SELEZIONA IL TUO COLORE:", start_x, eng_y, 1.1f, text_white);
            
            float col_btn_w = 225.0f;
            vec4 *col_w = (ui->selected_human_color == PLAYER_WHITE) ? &btn_active : &btn_normal;
            vec4 *col_b = (ui->selected_human_color == PLAYER_BLACK) ? &btn_active : &btn_normal;
            
            ui_add_quad(start_x, eng_y + 20.0f, col_btn_w, 40.0f, *col_w);
            ui_add_text_centered("GIOCA COME BIANCO", start_x + col_btn_w * 0.5f, eng_y + 40.0f, 1.1f, text_white);
            
            ui_add_quad(start_x + 240.0f, eng_y + 20.0f, col_btn_w, 40.0f, *col_b);
            ui_add_text_centered("GIOCA COME NERO", start_x + 240.0f + col_btn_w * 0.5f, eng_y + 40.0f, 1.1f, text_white);
            
            char cpu_str[128];
            snprintf(cpu_str, sizeof(cpu_str), "ENGINE CPU: %s", engine_get_type_name(ui->selected_black_engine));
            ui_add_quad(start_x, eng_y + 70.0f, 465.0f, 35.0f, btn_normal);
            ui_add_text_centered(cpu_str, start_x + 232.0f, eng_y + 87.0f, 1.1f, text_gold);
        } else if (ui->selected_mode == MODE_CPUVSCPU) {
            ui_add_text("SELEZIONA ENGINE CPU:", start_x, eng_y, 1.1f, text_white);
            
            char e1_str[64], e2_str[64];
            snprintf(e1_str, sizeof(e1_str), "CPU 1: %s", engine_get_type_name(ui->selected_white_engine));
            snprintf(e2_str, sizeof(e2_str), "CPU 2: %s", engine_get_type_name(ui->selected_black_engine));
            
            ui_add_quad(start_x, eng_y + 22.0f, 225.0f, 40.0f, btn_normal);
            ui_add_text_centered(e1_str, start_x + 112.0f, eng_y + 42.0f, 1.0f, text_gold);
            
            ui_add_quad(start_x + 240.0f, eng_y + 22.0f, 225.0f, 40.0f, btn_normal);
            ui_add_text_centered(e2_str, start_x + 240.0f + 112.0f, eng_y + 42.0f, 1.0f, text_gold);
        }
        
        // System status banner
        if (engine_is_type_available(ENGINE_TYPE_KINGSROW)) {
            ui_add_text("MOTORI DISPONIBILI: RANDOM, CHECKERBOARD (NATIVO), KINGSROW (BRIDGE)", start_x, p_y + 325.0f, 0.95f, text_sub);
        } else {
            ui_add_text("MOTORI DISPONIBILI: RANDOM, CHECKERBOARD (NATIVO MARTIN FIERZ)", start_x, p_y + 325.0f, 0.95f, text_sub);
        }
        
        // Start Game Button
        float start_btn_y = p_y + 355.0f;
        float start_btn_w = 320.0f;
        float start_btn_x = p_x + (p_w - start_btn_w) * 0.5f;
        
        ui_add_quad(start_btn_x, start_btn_y, start_btn_w, 55.0f, btn_start);
        ui_add_text_centered("INIZIA PARTITA", start_btn_x + start_btn_w * 0.5f, start_btn_y + 27.0f, 1.8f, text_white);
        
    } else if (ui->state == UI_STATE_PLAYING) {
        // Top Left Turn Box with solid dark background panel
        ui_add_quad(18.0f, 18.0f, 254.0f, 54.0f, border_color);
        ui_add_quad(20.0f, 20.0f, 250.0f, 50.0f, glass_panel);
        
        const char *turn_str = (game->current_player == PLAYER_WHITE) ? "TURNO: BIANCO" : "TURNO: NERO";
        ui_add_text_centered(turn_str, 145.0f, 45.0f, 1.4f, text_gold);
        
        // White Player / AI Response Time Badge
        float w_lbl_x = 285.0f;
        float lbl_w = 210.0f;
        float lbl_h = 50.0f;
        ui_add_quad(w_lbl_x - 2.0f, 18.0f, lbl_w + 4.0f, lbl_h + 4.0f, border_color);
        ui_add_quad(w_lbl_x, 20.0f, lbl_w, lbl_h, glass_panel);

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
            ui_add_text_centered(title_str, w_lbl_x + lbl_w * 0.5f, 34.0f, 0.95f, text_sub);
            ui_add_text_centered(val_str, w_lbl_x + lbl_w * 0.5f, 54.0f, 1.1f, text_gold);
        } else {
            ui_add_text_centered("BIANCO", w_lbl_x + lbl_w * 0.5f, 34.0f, 0.95f, text_sub);
            ui_add_text_centered("UMANO", w_lbl_x + lbl_w * 0.5f, 54.0f, 1.1f, text_white);
        }

        // Black Player / AI Response Time Badge
        float b_lbl_x = 510.0f;
        ui_add_quad(b_lbl_x - 2.0f, 18.0f, lbl_w + 4.0f, lbl_h + 4.0f, border_color);
        ui_add_quad(b_lbl_x, 20.0f, lbl_w, lbl_h, glass_panel);

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
            ui_add_text_centered(title_str, b_lbl_x + lbl_w * 0.5f, 34.0f, 0.95f, text_sub);
            ui_add_text_centered(val_str, b_lbl_x + lbl_w * 0.5f, 54.0f, 1.1f, text_gold);
        } else {
            ui_add_text_centered("NERO", b_lbl_x + lbl_w * 0.5f, 34.0f, 0.95f, text_sub);
            ui_add_text_centered("UMANO", b_lbl_x + lbl_w * 0.5f, 54.0f, 1.1f, text_white);
        }
        
        // Top Right Menu Button
        float menu_btn_x = (float)ui->win_w - 180.0f;
        ui_add_quad(menu_btn_x - 2.0f, 18.0f, 164.0f, 49.0f, border_color);
        ui_add_quad(menu_btn_x, 20.0f, 160.0f, 45.0f, btn_normal);
        ui_add_text_centered("MENU", menu_btn_x + 80.0f, 42.0f, 1.4f, text_white);
        
        // Game Over Overlay Modal with solid dark background panel
        if (game->is_game_over) {
            // Fullscreen backdrop overlay for modal
            ui_add_quad(0.0f, 0.0f, (float)ui->win_w, (float)ui->win_h, backdrop_overlay);
            
            float o_w = 420.0f;
            float o_h = 220.0f;
            float o_x = ((float)ui->win_w - o_w) * 0.5f;
            float o_y = ((float)ui->win_h - o_h) * 0.5f;
            
            ui_add_quad(o_x - 3.0f, o_y - 3.0f, o_w + 6.0f, o_h + 6.0f, border_color);
            ui_add_quad(o_x, o_y, o_w, o_h, glass_panel);
            
            ui_add_text_centered("PARTITA FINITA!", o_x + o_w * 0.5f, o_y + 40.0f, 2.0f, text_gold);
            
            const char *win_msg = (game->winner == PLAYER_WHITE) ? "HA VINTO IL BIANCO!" : "HA VINTO IL NERO!";
            ui_add_text_centered(win_msg, o_x + o_w * 0.5f, o_y + 85.0f, 1.5f, text_white);
            
            // Restart Button
            float r_w = 240.0f;
            float r_h = 50.0f;
            float r_x = o_x + (o_w - r_w) * 0.5f;
            float r_y = o_y + 135.0f;
            
            ui_add_quad(r_x, r_y, r_w, r_h, btn_start);
            ui_add_text_centered("NUOVA PARTITA", r_x + r_w * 0.5f, r_y + r_h * 0.5f, 1.4f, text_white);
        }
    }
    
    ui_end_and_draw(ui_shader, ui->win_w, ui->win_h);
}

bool ui_handle_click(UIContext *ui, GameState *game, double mouse_x, double mouse_y) {
    if (ui->state == UI_STATE_MAIN_MENU) {
        float p_w = 540.0f;
        float p_h = 440.0f;
        float p_x = ((float)ui->win_w - p_w) * 0.5f;
        float p_y = ((float)ui->win_h - p_h) * 0.5f;
        
        float btn_w = 145.0f;
        float btn_h = 45.0f;
        float start_x = p_x + 35.0f;
        float btn_y = p_y + 130.0f;
        
        // Mode 2Player
        if (point_in_rect(mouse_x, mouse_y, start_x, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_2PLAYER;
            return true;
        }
        // Mode 1Player
        if (point_in_rect(mouse_x, mouse_y, start_x + 160.0f, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_1PLAYER;
            return true;
        }
        // Mode CPU vs CPU
        if (point_in_rect(mouse_x, mouse_y, start_x + 320.0f, btn_y, btn_w, btn_h)) {
            ui->selected_mode = MODE_CPUVSCPU;
            return true;
        }
        
        // Color selection for 1Player mode
        if (ui->selected_mode == MODE_1PLAYER) {
            float eng_y = p_y + 195.0f;
            float col_btn_w = 225.0f;
            
            // White button
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 20.0f, col_btn_w, 40.0f)) {
                ui->selected_human_color = PLAYER_WHITE;
                return true;
            }
            // Black button
            if (point_in_rect(mouse_x, mouse_y, start_x + 240.0f, eng_y + 20.0f, col_btn_w, 40.0f)) {
                ui->selected_human_color = PLAYER_BLACK;
                return true;
            }

            // CPU Engine Selection button in 1Player mode
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 70.0f, 465.0f, 35.0f)) {
                ui->selected_black_engine = (ui->selected_black_engine + 1) % 3;
                if (!engine_is_type_available(ui->selected_black_engine)) {
                    ui->selected_black_engine = ENGINE_TYPE_RANDOM;
                }
                return true;
            }
        } else if (ui->selected_mode == MODE_CPUVSCPU) {
            float eng_y = p_y + 195.0f;
            if (point_in_rect(mouse_x, mouse_y, start_x, eng_y + 22.0f, 225.0f, 40.0f)) {
                ui->selected_white_engine = (ui->selected_white_engine + 1) % 3;
                if (!engine_is_type_available(ui->selected_white_engine)) {
                    ui->selected_white_engine = ENGINE_TYPE_RANDOM;
                }
                return true;
            }
            if (point_in_rect(mouse_x, mouse_y, start_x + 240.0f, eng_y + 22.0f, 225.0f, 40.0f)) {
                ui->selected_black_engine = (ui->selected_black_engine + 1) % 3;
                if (!engine_is_type_available(ui->selected_black_engine)) {
                    ui->selected_black_engine = ENGINE_TYPE_RANDOM;
                }
                return true;
            }
        }
        
        // Start Game Button
        float start_btn_y = p_y + 355.0f;
        float start_btn_w = 320.0f;
        float start_btn_x = p_x + (p_w - start_btn_w) * 0.5f;
        
        if (point_in_rect(mouse_x, mouse_y, start_btn_x, start_btn_y, start_btn_w, 55.0f)) {
            ui->state = UI_STATE_PLAYING;
            ui->has_white_ai_time = false;
            ui->has_black_ai_time = false;
            ui->last_white_ai_time = 0.0;
            ui->last_black_ai_time = 0.0;
            game_init(game, ui->selected_mode, ui->selected_human_color, ui->selected_white_engine, ui->selected_black_engine);
            return true;
        }
    } else if (ui->state == UI_STATE_PLAYING) {
        // Back to Main Menu Button
        float menu_btn_x = (float)ui->win_w - 180.0f;
        if (point_in_rect(mouse_x, mouse_y, menu_btn_x, 20.0f, 160.0f, 45.0f)) {
            ui->state = UI_STATE_MAIN_MENU;
            return true;
        }
        
        if (game->is_game_over) {
            float o_w = 420.0f;
            float o_h = 220.0f;
            float o_x = ((float)ui->win_w - o_w) * 0.5f;
            float o_y = ((float)ui->win_h - o_h) * 0.5f;
            
            float r_w = 240.0f;
            float r_h = 50.0f;
            float r_x = o_x + (o_w - r_w) * 0.5f;
            float r_y = o_y + 135.0f;
            
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
