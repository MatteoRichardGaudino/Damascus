#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

typedef enum {
    PIECE_NONE = 0,
    PIECE_WHITE_PAWN,
    PIECE_WHITE_DAMA,
    PIECE_BLACK_PAWN,
    PIECE_BLACK_DAMA
} PieceType;

typedef enum {
    PLAYER_WHITE = 0,
    PLAYER_BLACK = 1
} Player;

typedef enum {
    MODE_2PLAYER = 0,
    MODE_1PLAYER = 1,
    MODE_CPUVSCPU = 2
} GameMode;

typedef enum {
    ENGINE_TYPE_RANDOM = 0,
    ENGINE_TYPE_CHECKERBOARD = 1,
    ENGINE_TYPE_KINGSROW = 2
} EngineType;

typedef struct {
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    int captured_row; // -1 if simple move
    int captured_col; // -1 if simple move
    PieceType captured_type;
} Move;

typedef struct {
    PieceType board[8][8];
    Player current_player;
    GameMode mode;
    Player human_player; // PLAYER_WHITE or PLAYER_BLACK for 1P mode
    EngineType white_engine;
    EngineType black_engine;
    
    int white_eaten_count; // Black pieces captured
    int black_eaten_count; // White pieces captured
    
    PieceType white_eaten_list[12];
    PieceType black_eaten_list[12];

    int selected_row;
    int selected_col;
    
    bool is_game_over;
    Player winner;
} GameState;

void game_init(GameState *game, GameMode mode, Player human_player, EngineType white_engine, EngineType black_engine);
bool game_is_dark_tile(int row, int col);
bool game_is_valid_coord(int row, int col);

// Moves generation
int game_get_valid_moves(const GameState *game, Move *out_moves, int max_moves);
bool game_execute_move(GameState *game, const Move *move);

bool is_piece_white(PieceType piece);
bool is_piece_black(PieceType piece);
bool is_piece_dama(PieceType piece);

#endif // GAME_H
