#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <glad/glad.h>
#include "game.h"

typedef enum {
    UI_STATE_MAIN_MENU,
    UI_STATE_PLAYING
} UIState;

typedef struct {
    UIState state;
    GameMode selected_mode;
    Player selected_human_color; // PLAYER_WHITE or PLAYER_BLACK for 1P mode
    EngineType selected_white_engine;
    EngineType selected_black_engine;
    
    // UI layout bounds
    int win_w;
    int win_h;
    
    // AI Response Times for HUD labels
    double last_white_ai_time;
    double last_black_ai_time;
    bool has_white_ai_time;
    bool has_black_ai_time;
} UIContext;

void ui_init(UIContext *ui);
void ui_render(UIContext *ui, GameState *game, GLuint ui_shader);
bool ui_handle_click(UIContext *ui, GameState *game, double mouse_x, double mouse_y);

#endif // UI_H
