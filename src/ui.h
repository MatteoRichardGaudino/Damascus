#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <glad/glad.h>
#include "game.h"
#include "engine.h"

typedef enum {
    UI_STATE_MAIN_MENU,
    UI_STATE_PLAYING,
    UI_STATE_ENGINE_SETTINGS
} UIState;

typedef struct {
    UIState state;
    UIState prev_ui_state;
    GameMode selected_mode;
    Player selected_human_color; // PLAYER_WHITE or PLAYER_BLACK for 1P mode
    EngineType selected_white_engine;
    EngineType selected_black_engine;
    
    // Detailed Engine Parameters Configuration
    EngineConfig engine_config;
    int settings_tab; // 0 = MCTS UCB1, 1 = MCTS PUCT, 2 = CHECKERBOARD, 3 = KINGSROW

    // UI layout bounds
    int win_w;
    int win_h;

    // Engine Selection Dropdown / Radio Menu State
    bool engine_dropdown_open;
    int  dropdown_target_slot; // 0 = 1P CPU / White CPU, 1 = Black CPU
    float dropdown_x;
    float dropdown_y;
    float dropdown_w;
    float dropdown_h;

    // Direct Keyboard Time Input State
    bool   editing_time_input;
    int    editing_tab;
    char   time_input_buf[32];
    int    time_input_len;
    double editing_original_val;

    // Real-Time Live HUD Stats
    EngineStats white_stats;
    EngineStats black_stats;
    bool        is_thinking;
    double      thinking_start_time;
    Player      thinking_player;

    // Legacy AI response time fallback
    double last_white_ai_time;
    double last_black_ai_time;
    bool has_white_ai_time;
    bool has_black_ai_time;
} UIContext;

void ui_init(UIContext *ui);
void ui_render(UIContext *ui, GameState *game, GLuint ui_shader);
bool ui_handle_click(UIContext *ui, GameState *game, double mouse_x, double mouse_y);
void ui_handle_char(UIContext *ui, unsigned int codepoint);
void ui_handle_key(UIContext *ui, int key, int scancode, int action, int mods);

#endif // UI_H
