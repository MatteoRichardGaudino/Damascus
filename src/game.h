#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

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
    ENGINE_TYPE_KINGSROW = 2,
    ENGINE_TYPE_MCTS_UCB1 = 3
} EngineType;

// 128-bit Board representation
typedef struct {
    uint32_t white_men;   // 32 bit: Pedine bianche
    uint32_t white_kings; // 32 bit: Dame bianche
    uint32_t black_men;   // 32 bit: Pedine nere
    uint32_t black_kings; // 32 bit: Dame nere
} Board;

// 16-bit Move representation
// Bits 0..4  (5 bit): from_sq (0..31)
// Bits 5..9  (5 bit): to_sq   (0..31)
// Bits 10..11(2 bit): piece_type (0=PAWN, 1=DAMA)
// Bit 12     (1 bit): prom (1 if promotion)
// Bit 13     (1 bit): cap  (1 if capture)
// Bits 14..15(2 bit): reserved (0)
typedef uint16_t Move;

#define MOVE_NONE ((Move)0xFFFF)

#define MOVE_CREATE(from, to, type, prom, cap) \
    ((uint16_t)(((uint16_t)(from) & 0x1F) | \
    (((uint16_t)(to) & 0x1F) << 5) | \
    (((uint16_t)(type) & 0x3) << 10) | \
    (((uint16_t)(prom) & 0x1) << 12) | \
    (((uint16_t)(cap) & 0x1) << 13)))

#define MOVE_FROM(m)        ((uint8_t)((m) & 0x1F))
#define MOVE_TO(m)          ((uint8_t)(((m) >> 5) & 0x1F))
#define MOVE_TYPE(m)        ((uint8_t)(((m) >> 10) & 0x3))
#define MOVE_IS_PROM(m)     ((bool)(((m) >> 12) & 0x1))
#define MOVE_IS_CAP(m)      ((bool)(((m) >> 13) & 0x1))

// Coordinate conversions
#define SQ_TO_ROW(sq)       ((int)((sq) >> 2))
#define SQ_TO_COL(sq)       ((int)((((sq) & 3) << 1) + (((sq) >> 2) & 1)))
#define ROW_COL_TO_SQ(r, c) ((int)(((r) << 2) + ((c) >> 1)))

#define GET_CAPTURED_SQ(from, to) \
    ROW_COL_TO_SQ((SQ_TO_ROW(from) + SQ_TO_ROW(to)) >> 1, (SQ_TO_COL(from) + SQ_TO_COL(to)) >> 1)

// Board bitmask operations
#define BOARD_WHITE_PIECES(b) ((b).white_men | (b).white_kings)
#define BOARD_BLACK_PIECES(b) ((b).black_men | (b).black_kings)
#define BOARD_ALL_PIECES(b)   (BOARD_WHITE_PIECES(b) | BOARD_BLACK_PIECES(b))
#define BOARD_OCCUPIED(b)     BOARD_ALL_PIECES(b)
#define BOARD_FREE(b)         (~BOARD_OCCUPIED(b) & 0xFFFFFFFFU)

#define SQ_BIT(sq)            (1U << (sq))

// Move list structure (statically allocated)
typedef struct {
    uint16_t moves[32];
    uint8_t count;
} MoveList;

typedef struct {
    Board board;
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

// Board helper
PieceType board_get_piece_at(const Board *board, int sq);

// Moves generation and execution
const MoveList* game_get_valid_moves(const GameState *game);
bool game_execute_move(GameState *game, Move move);

bool is_piece_white(PieceType piece);
bool is_piece_black(PieceType piece);
bool is_piece_dama(PieceType piece);

#endif // GAME_H
