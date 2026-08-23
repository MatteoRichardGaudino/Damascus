#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "window.h"
#include "graphics.h"
#include "camera.h"
#include "game.h"
#include "engine.h"
#include "interaction.h"
#include "ui.h"
#include "cli.h"

static UIContext *s_global_ui = NULL;

static void char_callback(GLFWwindow *window, unsigned int codepoint) {
    (void)window;
    if (s_global_ui) {
        ui_handle_char(s_global_ui, codepoint);
    }
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window;
    if (s_global_ui) {
        ui_handle_key(s_global_ui, key, scancode, action, mods);
    }
}

typedef struct {
    Engine *engine;
    EngineType engine_type;
    GameState game_copy;
    Move result_move;
    volatile bool is_running;
    volatile bool finished;
    double start_time;
    Player player;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
} AIWorker;

#ifdef _WIN32
static DWORD WINAPI ai_thread_func(LPVOID lpParam) {
    AIWorker *worker = (AIWorker*)lpParam;
    if (worker && worker->engine && worker->engine->get_move) {
        worker->result_move = worker->engine->get_move(worker->engine->internal_state, &worker->game_copy);
    } else {
        worker->result_move = MOVE_NONE;
    }
    worker->finished = true;
    return 0;
}
#else
static void* ai_thread_func(void *param) {
    AIWorker *worker = (AIWorker*)param;
    if (worker && worker->engine && worker->engine->get_move) {
        worker->result_move = worker->engine->get_move(worker->engine->internal_state, &worker->game_copy);
    } else {
        worker->result_move = MOVE_NONE;
    }
    worker->finished = true;
    return NULL;
}
#endif

static void ai_worker_start(AIWorker *worker, Engine *engine, EngineType engine_type, const GameState *game, Player player) {
    worker->engine = engine;
    worker->engine_type = engine_type;
    worker->game_copy = *game;
    worker->result_move = MOVE_NONE;
    worker->finished = false;
    worker->is_running = true;
    worker->start_time = glfwGetTime();
    worker->player = player;

    engine_reset_stop();

#ifdef _WIN32
    worker->thread_handle = CreateThread(NULL, 0, ai_thread_func, worker, 0, NULL);
#else
    pthread_create(&worker->thread_handle, NULL, ai_thread_func, worker);
#endif
}

static void ai_worker_wait_and_close(AIWorker *worker) {
    if (!worker->is_running) return;
    engine_request_stop();
#ifdef _WIN32
    if (worker->thread_handle) {
        WaitForSingleObject(worker->thread_handle, 1000);
        CloseHandle(worker->thread_handle);
        worker->thread_handle = NULL;
    }
#else
    if (worker->thread_handle) {
        pthread_join(worker->thread_handle, NULL);
        worker->thread_handle = 0;
    }
#endif
    worker->is_running = false;
    worker->finished = false;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        bool force_gui = false;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--gui") == 0) {
                force_gui = true;
                break;
            }
        }
        if (!force_gui) {
            return cli_run(argc, argv);
        }
    }

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
    s_global_ui = &ui;

    glfwSetCharCallback(win.handle, char_callback);
    glfwSetKeyCallback(win.handle, key_callback);
    
    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_MCTS_UCB1, ENGINE_TYPE_MCTS_PUCT);
    
    Engine white_engine = engine_create(ENGINE_TYPE_MCTS_UCB1);
    Engine black_engine = engine_create(ENGINE_TYPE_MCTS_PUCT);
    engine_apply_config(&white_engine, ENGINE_TYPE_MCTS_UCB1, &ui.engine_config);
    engine_apply_config(&black_engine, ENGINE_TYPE_MCTS_PUCT, &ui.engine_config);
    
    PieceAnim piece_anim;
    piece_anim.active = false;

    AIWorker ai_worker;
    memset(&ai_worker, 0, sizeof(AIWorker));
    
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
        
        // Left Mouse Button Click Detection
        int left_state = glfwGetMouseButton(win.handle, GLFW_MOUSE_BUTTON_LEFT);
        if (left_state == GLFW_PRESS && !left_mouse_was_pressed) {
            left_mouse_was_pressed = true;
            
            UIState prev_state = ui.state;
            bool handled_by_ui = ui_handle_click(&ui, &game, mouse_x, mouse_y);
            
            // Check if UI transitioned states
            if (handled_by_ui && ui.state == UI_STATE_PLAYING && prev_state == UI_STATE_MAIN_MENU) {
                ai_worker_wait_and_close(&ai_worker);

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
                ai_worker_wait_and_close(&ai_worker);
                ui.is_thinking = false;
                camera_reset_menu_anim(&cam);
            } else if (handled_by_ui && ui.state == UI_STATE_ENGINE_SETTINGS && prev_state == UI_STATE_PLAYING) {
                ai_worker_wait_and_close(&ai_worker);
                ui.is_thinking = false;
            }
            
            if (!piece_anim.active && !handled_by_ui && ui.state == UI_STATE_PLAYING && !game.is_game_over && !ai_worker.is_running) {
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
        
        // CPU Asynchronous Move Execution Logic
        if (ui.state == UI_STATE_PLAYING && !game.is_game_over && !piece_anim.active) {
            bool is_cpu_turn = false;
            Engine *active_engine = NULL;
            
            EngineType active_type = ENGINE_TYPE_RANDOM;
            if (game.mode == MODE_CPUVSCPU) {
                is_cpu_turn = true;
                active_engine = (game.current_player == PLAYER_WHITE) ? &white_engine : &black_engine;
                active_type = (game.current_player == PLAYER_WHITE) ? game.white_engine : game.black_engine;
            } else if (game.mode == MODE_1PLAYER && game.current_player != game.human_player) {
                is_cpu_turn = true;
                active_engine = (game.human_player == PLAYER_WHITE) ? &black_engine : &white_engine;
                active_type = (game.human_player == PLAYER_WHITE) ? game.black_engine : game.white_engine;
            }
            
            if (ai_worker.is_running) {
                // Poll live stats while AI worker is calculating
                EngineStats live_st;
                engine_get_stats(ai_worker.engine, ai_worker.engine_type, &live_st);
                if (live_st.is_valid) {
                    if (ai_worker.player == PLAYER_WHITE) {
                        ui.white_stats = live_st;
                    } else {
                        ui.black_stats = live_st;
                    }
                }

                if (ai_worker.finished) {
#ifdef _WIN32
                    WaitForSingleObject(ai_worker.thread_handle, INFINITE);
                    CloseHandle(ai_worker.thread_handle);
                    ai_worker.thread_handle = NULL;
#else
                    pthread_join(ai_worker.thread_handle, NULL);
                    ai_worker.thread_handle = 0;
#endif
                    ai_worker.is_running = false;
                    ui.is_thinking = false;

                    double ai_elapsed = glfwGetTime() - ai_worker.start_time;
                    if (ai_worker.player == PLAYER_WHITE) {
                        ui.last_white_ai_time = ai_elapsed;
                        ui.has_white_ai_time = true;
                    } else {
                        ui.last_black_ai_time = ai_elapsed;
                        ui.has_black_ai_time = true;
                    }

                    EngineStats final_st;
                    engine_get_stats(ai_worker.engine, ai_worker.engine_type, &final_st);
                    if (final_st.is_valid) {
                        if (ai_worker.player == PLAYER_WHITE) {
                            ui.white_stats = final_st;
                        } else {
                            ui.black_stats = final_st;
                        }
                    }

                    if (!move_is_none(ai_worker.result_move) && !engine_is_stop_requested()) {
                        piece_anim_start(&piece_anim, &game, ai_worker.result_move, glfwGetTime());
                        game_execute_move(&game, ai_worker.result_move);
                        game.selected_row = -1;
                        game.selected_col = -1;
                    }
                    last_cpu_move_time = glfwGetTime();
                }
            } else if (is_cpu_turn && active_engine) {
                float delay = (game.mode == MODE_CPUVSCPU) ? 0.20f : 0.35f;
                
                if (current_time - last_cpu_move_time >= delay) {
                    ui.is_thinking = true;
                    ui.thinking_start_time = glfwGetTime();
                    ui.thinking_player = game.current_player;
                    ai_worker_start(&ai_worker, active_engine, active_type, &game, game.current_player);
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
    
    ai_worker_wait_and_close(&ai_worker);
    engine_destroy(&white_engine);
    engine_destroy(&black_engine);
    graphics_cleanup(&gfx);
    window_cleanup(&win);
    
    return 0;
}
