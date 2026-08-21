#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "window.h"
#include "graphics.h"
#include "camera.h"
#include "game.h"
#include "engine.h"
#include "interaction.h"
#include "ui.h"

int main(void) {
    Window win;
    if (!window_init(&win, "Damascus - Dama Italiana 3D", 1024, 768)) {
        return -1;
    }
    
    GraphicsContext gfx;
    if (!graphics_init(&gfx)) {
        window_cleanup(&win);
        return -1;
    }
    
    Camera cam;
    camera_init(&cam);
    
    UIContext ui;
    ui_init(&ui);
    
    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);
    
    Engine white_engine = engine_create(ENGINE_TYPE_RANDOM);
    Engine black_engine = engine_create(ENGINE_TYPE_RANDOM);
    
    PieceAnim piece_anim;
    piece_anim.active = false;
    
    double last_mouse_x = 0.0, last_mouse_y = 0.0;
    bool right_mouse_pressed = false;
    bool left_mouse_was_pressed = false;
    
    double last_cpu_move_time = 0.0;
    double last_frame_time = glfwGetTime();
    
    while (!window_should_close(&win)) {
        window_poll_events();
        
        double current_time = glfwGetTime();
        float delta_time = (float)(current_time - last_frame_time);
        last_frame_time = current_time;
        
        // Update camera animations (zoom sweep & orientation interpolation)
        camera_update(&cam, delta_time);
        
        ui.win_w = win.win_width;
        ui.win_h = win.win_height;
        
        double mouse_x, mouse_y;
        glfwGetCursorPos(win.handle, &mouse_x, &mouse_y);
        
        // Right Mouse Button: Camera Rotation
        int right_state = glfwGetMouseButton(win.handle, GLFW_MOUSE_BUTTON_RIGHT);
        if (right_state == GLFW_PRESS) {
            if (!right_mouse_pressed) {
                right_mouse_pressed = true;
                last_mouse_x = mouse_x;
                last_mouse_y = mouse_y;
            } else {
                double dx = mouse_x - last_mouse_x;
                double dy = mouse_y - last_mouse_y;
                camera_update_rotation(&cam, dx, dy);
                last_mouse_x = mouse_x;
                last_mouse_y = mouse_y;
            }
        } else {
            right_mouse_pressed = false;
        }
        
        // Left Mouse Button Click Detection (Edge triggered on release or press)
        int left_state = glfwGetMouseButton(win.handle, GLFW_MOUSE_BUTTON_LEFT);
        if (left_state == GLFW_PRESS && !left_mouse_was_pressed) {
            left_mouse_was_pressed = true;
            
            UIState prev_state = ui.state;
            bool handled_by_ui = ui_handle_click(&ui, &game, mouse_x, mouse_y);
            
            // Check if UI transitioned to PLAYING -> trigger camera sweep animation!
            if (handled_by_ui && ui.state == UI_STATE_PLAYING && prev_state == UI_STATE_MAIN_MENU) {
                Player view_player = (game.mode == MODE_1PLAYER) ? game.human_player : PLAYER_WHITE;
                camera_start_game_anim(&cam, view_player);

                engine_destroy(&white_engine);
                engine_destroy(&black_engine);
                white_engine = engine_create(game.white_engine);
                black_engine = engine_create(game.black_engine);
                engine_apply_config(&white_engine, game.white_engine, &ui.engine_config);
                engine_apply_config(&black_engine, game.black_engine, &ui.engine_config);
            } else if (handled_by_ui && ui.state == UI_STATE_PLAYING && prev_state == UI_STATE_ENGINE_SETTINGS) {
                engine_apply_config(&white_engine, game.white_engine, &ui.engine_config);
                engine_apply_config(&black_engine, game.black_engine, &ui.engine_config);
            } else if (handled_by_ui && ui.state == UI_STATE_MAIN_MENU && prev_state == UI_STATE_PLAYING) {
                camera_reset_menu_anim(&cam);
            }

            
            if (!piece_anim.active && !handled_by_ui && ui.state == UI_STATE_PLAYING && !game.is_game_over) {
                // Determine if current player is Human
                bool is_human = false;
                if (game.mode == MODE_2PLAYER) {
                    is_human = true;
                } else if (game.mode == MODE_1PLAYER && game.current_player == game.human_player) {
                    is_human = true;
                }
                
                if (is_human) {
                    int row, col;
                    if (interaction_pick_tile(mouse_x, mouse_y, win.win_width, win.win_height, &cam, &row, &col)) {
                        const MoveList *valid_moves = game_get_valid_moves(&game);
                        int picked_sq = ROW_COL_TO_SQ(row, col);
                        
                        if (game.selected_row < 0) {
                            // Select piece
                            if (game_is_dark_tile(row, col)) {
                                PieceType p = board_get_piece_at(&game.board, picked_sq);
                                bool matches_player = (game.current_player == PLAYER_WHITE) ? is_piece_white(p) : is_piece_black(p);
                                if (matches_player) {
                                    game.selected_row = row;
                                    game.selected_col = col;
                                }
                            }
                        } else {
                            // Execute move if tile is valid
                            bool move_executed = false;
                            int sel_sq = ROW_COL_TO_SQ(game.selected_row, game.selected_col);
                            
                            if (game_is_dark_tile(row, col)) {
                                for (int m = 0; m < valid_moves->count; m++) {
                                    Move mv = valid_moves->moves[m];
                                    if (MOVE_FROM(mv) == sel_sq && MOVE_TO(mv) == picked_sq) {
                                        piece_anim_start(&piece_anim, &game, mv, current_time);
                                        game_execute_move(&game, mv);
                                        game.selected_row = -1;
                                        game.selected_col = -1;
                                        move_executed = true;
                                        break;
                                    }
                                }
                            }
                            if (!move_executed) {
                                // Select new piece or deselect
                                if (game_is_dark_tile(row, col)) {
                                    PieceType p = board_get_piece_at(&game.board, picked_sq);
                                    bool matches_player = (game.current_player == PLAYER_WHITE) ? is_piece_white(p) : is_piece_black(p);
                                    if (matches_player) {
                                        game.selected_row = row;
                                        game.selected_col = col;
                                    } else {
                                        game.selected_row = -1;
                                        game.selected_col = -1;
                                    }
                                } else {
                                    game.selected_row = -1;
                                    game.selected_col = -1;
                                }
                            }
                        }
                    }
                }
            }
        } else if (left_state == GLFW_RELEASE) {
            left_mouse_was_pressed = false;
        }
        
        // CPU Automatic Move Logic
        if (ui.state == UI_STATE_PLAYING && !game.is_game_over && !piece_anim.active) {
            bool is_cpu_turn = false;
            Engine *active_engine = NULL;
            
            if (game.mode == MODE_CPUVSCPU) {
                is_cpu_turn = true;
                active_engine = (game.current_player == PLAYER_WHITE) ? &white_engine : &black_engine;
            } else if (game.mode == MODE_1PLAYER && game.current_player != game.human_player) {
                is_cpu_turn = true;
                active_engine = (game.human_player == PLAYER_WHITE) ? &black_engine : &white_engine;
            }
            
            if (is_cpu_turn && active_engine) {
                float delay = (game.mode == MODE_CPUVSCPU) ? 0.20f : 0.35f;
                
                if (current_time - last_cpu_move_time >= delay) {
                    Player moving_player = game.current_player;
                    double ai_start = glfwGetTime();
                    Move cpu_move = active_engine->get_move(active_engine->internal_state, &game);
                    double ai_elapsed = glfwGetTime() - ai_start;

                    if (moving_player == PLAYER_WHITE) {
                        ui.last_white_ai_time = ai_elapsed;
                        ui.has_white_ai_time = true;
                    } else {
                        ui.last_black_ai_time = ai_elapsed;
                        ui.has_black_ai_time = true;
                    }

                    if (!move_is_none(cpu_move)) {
                        piece_anim_start(&piece_anim, &game, cpu_move, glfwGetTime());
                        game_execute_move(&game, cpu_move);
                        game.selected_row = -1;
                        game.selected_col = -1;
                    }
                    last_cpu_move_time = glfwGetTime();
                }
            }
        }
        
        // Fetch valid moves for visual highlighting
        const MoveList *current_valid_moves = game_get_valid_moves(&game);
        
        // Render Frame
        glClearColor(0.05f, 0.07f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        graphics_render_scene(&gfx, &game, &cam, win.aspect_ratio, current_valid_moves, &piece_anim, current_time);
        ui_render(&ui, &game, gfx.ui_shader);
        
        window_swap_buffers(&win);
    }
    
    engine_destroy(&white_engine);
    engine_destroy(&black_engine);
    graphics_cleanup(&gfx);
    window_cleanup(&win);
    
    return 0;
}
