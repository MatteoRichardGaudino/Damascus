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
    ENGINE_TYPE_MCTS_UCB1 = 3,
    ENGINE_TYPE_MCTS_PUCT = 4,
    ENGINE_TYPE_COUNT = 5
} EngineType;

// 128-bit Board representation
typedef struct {
    uint32_t white_men;   // 32 bit: Pedine bianche
    uint32_t white_kings; // 32 bit: Dame bianche
    uint32_t black_men;   // 32 bit: Pedine nere
    uint32_t black_kings; // 32 bit: Dame nere
} Board;

// Rich Move representation supporting multi-jumps (Dama Italiana FID)
typedef struct {
    uint8_t  from;       // Starting square (0..31, or 0xFF for MOVE_NONE)
    uint8_t  to;         // Final destination square (0..31)
    uint8_t  piece_type; // 0 = PAWN, 1 = DAMA
    uint8_t  is_prom;    // 1 if promoted to Dama during the move
    uint8_t  is_cap;     // 1 if capture move
    uint8_t  jumps;      // Number of jumped pieces (0 for quiet move, 1..N for capture)
    uint8_t  path[8];    // Intermediate path: path[0]=from, path[1..jumps]=landing squares
    uint8_t  caps[8];    // Squares of captured pieces in chronological order
    uint32_t cap_mask;   // Bitmask of captured squares (1U << cap_sq)
} Move;

#define MOVE_NONE ((Move){ .from = 0xFF, .to = 0xFF, .piece_type = 0, .is_prom = 0, .is_cap = 0, .jumps = 0, .cap_mask = 0 })

static inline bool move_is_none(Move m) {
    return m.from == 0xFF || m.from > 31;
}

static inline bool move_equals(Move a, Move b) {
    if (move_is_none(a) && move_is_none(b)) return true;
    if (a.from != b.from || a.to != b.to) return false;
    if (a.is_cap != b.is_cap || a.jumps != b.jumps) return false;
    return a.cap_mask == b.cap_mask;
}

#define MOVE_FROM(m)        ((m).from)
#define MOVE_TO(m)          ((m).to)
#define MOVE_TYPE(m)        ((m).piece_type)
#define MOVE_IS_PROM(m)     ((bool)((m).is_prom))
#define MOVE_IS_CAP(m)      ((bool)((m).is_cap))

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
    Move    moves[48];
    uint8_t count;
} MoveList;

#define MAX_GAME_HISTORY 512

typedef struct {
    uint32_t wm;
    uint32_t wk;
    uint32_t bm;
    uint32_t bk;
    uint8_t  player;
    uint64_t hash;
} PositionKey;

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
    bool is_draw;
    Player winner;
    
    uint64_t hash; // 64-bit Zobrist hash of board and player
    
    int history_count;
    PositionKey history[MAX_GAME_HISTORY];
} GameState;

void game_init(GameState *game, GameMode mode, Player human_player, EngineType white_engine, EngineType black_engine);
bool game_is_dark_tile(int row, int col);
bool game_is_valid_coord(int row, int col);

// Board helper
PieceType board_get_piece_at(const Board *board, int sq);

// Moves generation and execution
const MoveList* game_get_valid_moves(const GameState *game);
bool game_execute_move(GameState *game, Move move);

// Draw and Repetition checkers
int game_get_repetition_count(const GameState *game);
bool game_is_threefold_repetition(const GameState *game);

bool is_piece_white(PieceType piece);
bool is_piece_black(PieceType piece);
bool is_piece_dama(PieceType piece);

#endif // GAME_H

