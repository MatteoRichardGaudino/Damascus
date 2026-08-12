#include "game.h"
#include <string.h>
#include <stdlib.h>

bool is_piece_white(PieceType piece) {
    return piece == PIECE_WHITE_PAWN || piece == PIECE_WHITE_DAMA;
}

bool is_piece_black(PieceType piece) {
    return piece == PIECE_BLACK_PAWN || piece == PIECE_BLACK_DAMA;
}

bool is_piece_dama(PieceType piece) {
    return piece == PIECE_WHITE_DAMA || piece == PIECE_BLACK_DAMA;
}

bool game_is_dark_tile(int row, int col) {
    return (row + col) % 2 == 1;
}

bool game_is_valid_coord(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

void game_init(GameState *game, GameMode mode, Player human_player, EngineType white_engine, EngineType black_engine) {
    memset(game, 0, sizeof(GameState));
    game->mode = mode;
    game->human_player = human_player;
    game->white_engine = white_engine;
    game->black_engine = black_engine;
    game->current_player = PLAYER_WHITE;
    game->selected_row = -1;
    game->selected_col = -1;
    game->is_game_over = false;
    
    // Set up Italian Checkers initial board
    // White pieces at rows 0, 1, 2 on dark tiles
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 8; c++) {
            if (game_is_dark_tile(r, c)) {
                game->board[r][c] = PIECE_WHITE_PAWN;
            }
        }
    }
    
    // Black pieces at rows 5, 6, 7 on dark tiles
    for (int r = 5; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (game_is_dark_tile(r, c)) {
                game->board[r][c] = PIECE_BLACK_PAWN;
            }
        }
    }
}

static void evaluate_captures_for_piece(const GameState *game, int r, int c, Move *captures, int *count, int max_moves) {
    PieceType p = game->board[r][c];
    if (p == PIECE_NONE) return;
    
    Player cur = game->current_player;
    if (cur == PLAYER_WHITE && !is_piece_white(p)) return;
    if (cur == PLAYER_BLACK && !is_piece_black(p)) return;

    bool is_dama = is_piece_dama(p);
    
    // Direction vectors for diagonals
    int dr[4], dc[4];
    int dir_count = 0;
    
    if (is_dama) {
        dr[0] = 1;  dc[0] = 1;
        dr[1] = 1;  dc[1] = -1;
        dr[2] = -1; dc[2] = 1;
        dr[3] = -1; dc[3] = -1;
        dir_count = 4;
    } else {
        // Pawn moves forward only
        // White moves UP (+1 row), Black moves DOWN (-1 row)
        int forward = (cur == PLAYER_WHITE) ? 1 : -1;
        dr[0] = forward; dc[0] = 1;
        dr[1] = forward; dc[1] = -1;
        dir_count = 2;
    }
    
    for (int i = 0; i < dir_count; i++) {
        int mid_r = r + dr[i];
        int mid_c = c + dc[i];
        int land_r = r + 2 * dr[i];
        int land_c = c + 2 * dc[i];
        
        if (game_is_valid_coord(mid_r, mid_c) && game_is_valid_coord(land_r, land_c)) {
            PieceType enemy = game->board[mid_r][mid_c];
            PieceType landing = game->board[land_r][land_c];
            
            bool is_enemy = (cur == PLAYER_WHITE) ? is_piece_black(enemy) : is_piece_white(enemy);
            
            // Italian Rule: Pawn CANNOT capture a Dama
            if (!is_dama && is_piece_dama(enemy)) {
                is_enemy = false;
            }
            
            if (is_enemy && landing == PIECE_NONE) {
                if (*count < max_moves) {
                    Move m;
                    m.from_row = r;
                    m.from_col = c;
                    m.to_row = land_r;
                    m.to_col = land_c;
                    m.captured_row = mid_r;
                    m.captured_col = mid_c;
                    m.captured_type = enemy;
                    captures[(*count)++] = m;
                }
            }
        }
    }
}

static void evaluate_simple_moves_for_piece(const GameState *game, int r, int c, Move *moves, int *count, int max_moves) {
    PieceType p = game->board[r][c];
    if (p == PIECE_NONE) return;
    
    Player cur = game->current_player;
    if (cur == PLAYER_WHITE && !is_piece_white(p)) return;
    if (cur == PLAYER_BLACK && !is_piece_black(p)) return;

    bool is_dama = is_piece_dama(p);
    int dr[4], dc[4];
    int dir_count = 0;
    
    if (is_dama) {
        dr[0] = 1;  dc[0] = 1;
        dr[1] = 1;  dc[1] = -1;
        dr[2] = -1; dc[2] = 1;
        dr[3] = -1; dc[3] = -1;
        dir_count = 4;
    } else {
        int forward = (cur == PLAYER_WHITE) ? 1 : -1;
        dr[0] = forward; dc[0] = 1;
        dr[1] = forward; dc[1] = -1;
        dir_count = 2;
    }
    
    for (int i = 0; i < dir_count; i++) {
        int nr = r + dr[i];
        int nc = c + dc[i];
        
        if (game_is_valid_coord(nr, nc) && game->board[nr][nc] == PIECE_NONE) {
            if (*count < max_moves) {
                Move m;
                m.from_row = r;
                m.from_col = c;
                m.to_row = nr;
                m.to_col = nc;
                m.captured_row = -1;
                m.captured_col = -1;
                m.captured_type = PIECE_NONE;
                moves[(*count)++] = m;
            }
        }
    }
}

int game_get_valid_moves(const GameState *game, Move *out_moves, int max_moves) {
    if (game->is_game_over) return 0;

    Move captures[128];
    int capture_count = 0;
    
    // First check for captures (captures are mandatory in Italian Checkers)
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            evaluate_captures_for_piece(game, r, c, captures, &capture_count, 128);
        }
    }
    
    if (capture_count > 0) {
        // In Italian Checkers (Law of Maximum):
        // Filter capture list by priority:
        // 1. Must capture maximum pieces (for now 1-step capture evaluation, we pick captures).
        // 2. Captures made by Dama prioritized over Pawns if equal.
        bool has_dama_capture = false;
        for (int i = 0; i < capture_count; i++) {
            PieceType p = game->board[captures[i].from_row][captures[i].from_col];
            if (is_piece_dama(p)) {
                has_dama_capture = true;
                break;
            }
        }
        
        int final_count = 0;
        for (int i = 0; i < capture_count; i++) {
            PieceType p = game->board[captures[i].from_row][captures[i].from_col];
            if (has_dama_capture && !is_piece_dama(p)) {
                continue; // Skip pawn capture if dama capture is available
            }
            if (final_count < max_moves) {
                out_moves[final_count++] = captures[i];
            }
        }
        return final_count;
    }
    
    // No captures available -> generate simple moves
    int simple_count = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            evaluate_simple_moves_for_piece(game, r, c, out_moves, &simple_count, max_moves);
        }
    }
    return simple_count;
}

bool game_execute_move(GameState *game, const Move *move) {
    PieceType p = game->board[move->from_row][move->from_col];
    if (p == PIECE_NONE) return false;
    
    // Move piece
    game->board[move->from_row][move->from_col] = PIECE_NONE;
    
    // Check for Dama promotion (reaching opposite baseline)
    if (p == PIECE_WHITE_PAWN && move->to_row == 7) {
        p = PIECE_WHITE_DAMA;
    } else if (p == PIECE_BLACK_PAWN && move->to_row == 0) {
        p = PIECE_BLACK_DAMA;
    }
    
    game->board[move->to_row][move->to_col] = p;
    
    // Handle capture
    if (move->captured_row >= 0 && move->captured_col >= 0) {
        PieceType cap = move->captured_type;
        game->board[move->captured_row][move->captured_col] = PIECE_NONE;
        
        if (game->current_player == PLAYER_WHITE) {
            if (game->white_eaten_count < 12) {
                game->white_eaten_list[game->white_eaten_count++] = cap;
            }
        } else {
            if (game->black_eaten_count < 12) {
                game->black_eaten_list[game->black_eaten_count++] = cap;
            }
        }
    }
    
    // Switch turn
    game->current_player = (game->current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Check if next player has any valid moves
    Move test_moves[128];
    int next_moves = game_get_valid_moves(game, test_moves, 128);
    if (next_moves == 0) {
        game->is_game_over = true;
        // The current player after switch has no moves -> previous player won!
        game->winner = (game->current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    }
    
    return true;
}
