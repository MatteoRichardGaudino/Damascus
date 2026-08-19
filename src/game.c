#include "game.h"
#include <string.h>
#include <stdlib.h>

// Precomputed geometric lookup tables for 32 dark squares (4 directions)
// Dir 0: Up-Right (+1 row, +1 col)
// Dir 1: Up-Left  (+1 row, -1 col)
// Dir 2: Down-Right (-1 row, +1 col)
// Dir 3: Down-Left  (-1 row, -1 col)
static int8_t s_adj[32][4];
static int8_t s_jump_dest[32][4];
static int8_t s_jump_mid[32][4];
static bool s_tables_initialized = false;

// Statically allocated MoveList (allocated once for memory efficiency)
static MoveList g_move_list;

static void init_tables(void) {
    if (s_tables_initialized) return;
    
    for (int sq = 0; sq < 32; sq++) {
        int r = SQ_TO_ROW(sq);
        int c = SQ_TO_COL(sq);
        
        int dr[4] = { 1,  1, -1, -1 };
        int dc[4] = { 1, -1,  1, -1 };
        
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d];
            int nc = c + dc[d];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                s_adj[sq][d] = (int8_t)ROW_COL_TO_SQ(nr, nc);
            } else {
                s_adj[sq][d] = -1;
            }
            
            int jr = r + 2 * dr[d];
            int jc = c + 2 * dc[d];
            if (jr >= 0 && jr < 8 && jc >= 0 && jc < 8) {
                s_jump_dest[sq][d] = (int8_t)ROW_COL_TO_SQ(jr, jc);
                s_jump_mid[sq][d] = (int8_t)ROW_COL_TO_SQ(nr, nc);
            } else {
                s_jump_dest[sq][d] = -1;
                s_jump_mid[sq][d] = -1;
            }
        }
    }
    s_tables_initialized = true;
}

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
    return (row + col) % 2 == 0;
}

bool game_is_valid_coord(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

PieceType board_get_piece_at(const Board *board, int sq) {
    if (sq < 0 || sq > 31) return PIECE_NONE;
    uint32_t mask = 1U << sq;
    if (board->white_men & mask) return PIECE_WHITE_PAWN;
    if (board->white_kings & mask) return PIECE_WHITE_DAMA;
    if (board->black_men & mask) return PIECE_BLACK_PAWN;
    if (board->black_kings & mask) return PIECE_BLACK_DAMA;
    return PIECE_NONE;
}

void game_init(GameState *game, GameMode mode, Player human_player, EngineType white_engine, EngineType black_engine) {
    init_tables();
    memset(game, 0, sizeof(GameState));
    game->mode = mode;
    game->human_player = human_player;
    game->white_engine = white_engine;
    game->black_engine = black_engine;
    game->current_player = PLAYER_WHITE;
    game->selected_row = -1;
    game->selected_col = -1;
    game->is_game_over = false;
    
    // Initial Italian Checkers 128-bit bitboard setup:
    // White pieces at rows 0, 1, 2 (sq 0..11 -> bits 0..11)
    game->board.white_men = 0x00000FFFU;
    game->board.white_kings = 0;
    // Black pieces at rows 5, 6, 7 (sq 20..31 -> bits 20..31)
    game->board.black_men = 0xFFF00000U;
    game->board.black_kings = 0;
}

const MoveList* game_get_valid_moves(const GameState *game) {
    init_tables();
    g_move_list.count = 0;
    if (!game || game->is_game_over) return &g_move_list;

    const Board *b = &game->board;
    Player player = game->current_player;
    uint32_t occ = BOARD_OCCUPIED(*b);
    uint32_t free_mask = ~occ;

    // Separate captures buffer to filter according to Italian Checkers rules
    uint16_t captures[32];
    uint8_t capture_count = 0;
    bool has_dama_capture = false;

    if (player == PLAYER_WHITE) {
        // White Pawns captures (Up-Right = dir 0, Up-Left = dir 1)
        // Rule: White Pawns can ONLY capture Black Pawns (NOT Black Kings)
        uint32_t my_men = b->white_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1; // Clear lowest set bit
            
            for (int d = 0; d < 2; d++) {
                int mid = s_jump_mid[sq][d];
                int dest = s_jump_dest[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    // Check if mid contains enemy pawn
                    if (b->black_men & (1U << mid)) {
                        bool is_prom = (dest >= 28);
                        Move m = MOVE_CREATE(sq, dest, 0, is_prom ? 1 : 0, 1);
                        if (capture_count < 32) {
                            captures[capture_count++] = m;
                        }
                    }
                }
            }
        }

        // White Kings captures (All 4 directions: dir 0..3)
        // Rule: White Kings can capture any Black piece (men or kings)
        uint32_t my_kings = b->white_kings;
        uint32_t enemy_all = b->black_men | b->black_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            
            for (int d = 0; d < 4; d++) {
                int mid = s_jump_mid[sq][d];
                int dest = s_jump_dest[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    if (enemy_all & (1U << mid)) {
                        Move m = MOVE_CREATE(sq, dest, 1, 0, 1);
                        has_dama_capture = true;
                        if (capture_count < 32) {
                            captures[capture_count++] = m;
                        }
                    }
                }
            }
        }
    } else {
        // Black Pawns captures (Down-Right = dir 2, Down-Left = dir 3)
        // Rule: Black Pawns can ONLY capture White Pawns (NOT White Kings)
        uint32_t my_men = b->black_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1;
            
            for (int d = 2; d < 4; d++) {
                int mid = s_jump_mid[sq][d];
                int dest = s_jump_dest[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    if (b->white_men & (1U << mid)) {
                        bool is_prom = (dest <= 3);
                        Move m = MOVE_CREATE(sq, dest, 0, is_prom ? 1 : 0, 1);
                        if (capture_count < 32) {
                            captures[capture_count++] = m;
                        }
                    }
                }
            }
        }

        // Black Kings captures (All 4 directions: dir 0..3)
        uint32_t my_kings = b->black_kings;
        uint32_t enemy_all = b->white_men | b->white_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            
            for (int d = 0; d < 4; d++) {
                int mid = s_jump_mid[sq][d];
                int dest = s_jump_dest[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    if (enemy_all & (1U << mid)) {
                        Move m = MOVE_CREATE(sq, dest, 1, 0, 1);
                        has_dama_capture = true;
                        if (capture_count < 32) {
                            captures[capture_count++] = m;
                        }
                    }
                }
            }
        }
    }

    // Italian Checkers: Captures are mandatory
    if (capture_count > 0) {
        for (int i = 0; i < capture_count; i++) {
            Move m = captures[i];
            // If Dama capture is available, filter out pawn captures
            if (has_dama_capture && MOVE_TYPE(m) != 1) {
                continue;
            }
            if (g_move_list.count < 32) {
                g_move_list.moves[g_move_list.count++] = m;
            }
        }
        return &g_move_list;
    }

    // No captures -> generate simple moves
    if (player == PLAYER_WHITE) {
        // White Pawns simple moves (dir 0, 1)
        uint32_t my_men = b->white_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1;
            
            for (int d = 0; d < 2; d++) {
                int dest = s_adj[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    bool is_prom = (dest >= 28);
                    Move m = MOVE_CREATE(sq, dest, 0, is_prom ? 1 : 0, 0);
                    if (g_move_list.count < 32) {
                        g_move_list.moves[g_move_list.count++] = m;
                    }
                }
            }
        }

        // White Kings simple moves (dir 0..3)
        uint32_t my_kings = b->white_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            
            for (int d = 0; d < 4; d++) {
                int dest = s_adj[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    Move m = MOVE_CREATE(sq, dest, 1, 0, 0);
                    if (g_move_list.count < 32) {
                        g_move_list.moves[g_move_list.count++] = m;
                    }
                }
            }
        }
    } else {
        // Black Pawns simple moves (dir 2, 3)
        uint32_t my_men = b->black_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1;
            
            for (int d = 2; d < 4; d++) {
                int dest = s_adj[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    bool is_prom = (dest <= 3);
                    Move m = MOVE_CREATE(sq, dest, 0, is_prom ? 1 : 0, 0);
                    if (g_move_list.count < 32) {
                        g_move_list.moves[g_move_list.count++] = m;
                    }
                }
            }
        }

        // Black Kings simple moves (dir 0..3)
        uint32_t my_kings = b->black_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            
            for (int d = 0; d < 4; d++) {
                int dest = s_adj[sq][d];
                if (dest >= 0 && (free_mask & (1U << dest))) {
                    Move m = MOVE_CREATE(sq, dest, 1, 0, 0);
                    if (g_move_list.count < 32) {
                        g_move_list.moves[g_move_list.count++] = m;
                    }
                }
            }
        }
    }

    return &g_move_list;
}

bool game_execute_move(GameState *game, Move move) {
    if (move == MOVE_NONE) return false;
    
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    bool is_prom = MOVE_IS_PROM(move);
    bool is_cap = MOVE_IS_CAP(move);
    
    uint32_t from_mask = 1U << from;
    uint32_t to_mask = 1U << to;
    Board *b = &game->board;
    Player cur = game->current_player;
    
    if (cur == PLAYER_WHITE) {
        if (b->white_men & from_mask) {
            b->white_men &= ~from_mask;
            if (is_prom || to >= 28) {
                b->white_kings |= to_mask;
            } else {
                b->white_men |= to_mask;
            }
        } else if (b->white_kings & from_mask) {
            b->white_kings &= ~from_mask;
            b->white_kings |= to_mask;
        } else {
            return false;
        }
        
        if (is_cap) {
            int cap_sq = GET_CAPTURED_SQ(from, to);
            uint32_t cap_mask = 1U << cap_sq;
            PieceType cap_type = PIECE_NONE;
            
            if (b->black_men & cap_mask) {
                b->black_men &= ~cap_mask;
                cap_type = PIECE_BLACK_PAWN;
            } else if (b->black_kings & cap_mask) {
                b->black_kings &= ~cap_mask;
                cap_type = PIECE_BLACK_DAMA;
            }
            
            if (game->white_eaten_count < 12) {
                game->white_eaten_list[game->white_eaten_count++] = cap_type;
            }
        }
    } else {
        if (b->black_men & from_mask) {
            b->black_men &= ~from_mask;
            if (is_prom || to <= 3) {
                b->black_kings |= to_mask;
            } else {
                b->black_men |= to_mask;
            }
        } else if (b->black_kings & from_mask) {
            b->black_kings &= ~from_mask;
            b->black_kings |= to_mask;
        } else {
            return false;
        }
        
        if (is_cap) {
            int cap_sq = GET_CAPTURED_SQ(from, to);
            uint32_t cap_mask = 1U << cap_sq;
            PieceType cap_type = PIECE_NONE;
            
            if (b->white_men & cap_mask) {
                b->white_men &= ~cap_mask;
                cap_type = PIECE_WHITE_PAWN;
            } else if (b->white_kings & cap_mask) {
                b->white_kings &= ~cap_mask;
                cap_type = PIECE_WHITE_DAMA;
            }
            
            if (game->black_eaten_count < 12) {
                game->black_eaten_list[game->black_eaten_count++] = cap_type;
            }
        }
    }
    
    // Switch turn
    game->current_player = (cur == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Check if next player has moves
    const MoveList *next_moves = game_get_valid_moves(game);
    if (next_moves->count == 0) {
        game->is_game_over = true;
        game->winner = cur; // Current player won!
    }
    
    return true;
}

