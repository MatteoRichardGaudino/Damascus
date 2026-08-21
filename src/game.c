#include "game.h"
#include "zobrist.h"
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

// Statically allocated MoveList (allocated per-thread for memory efficiency and concurrency)
static _Thread_local MoveList g_move_list;

typedef struct {
    uint8_t  from;
    uint8_t  to;
    uint8_t  piece_type; // 0 = Pawn, 1 = King
    uint8_t  is_prom;
    uint8_t  jumps;
    uint8_t  path[8];
    uint8_t  caps[8];
    uint32_t cap_mask;
    
    // Priority metrics for Law of Maximum:
    uint8_t  king_caps;        // Number of enemy kings captured
    uint8_t  first_king_step;  // Step index (1..jumps) of first king captured, or 99 if none
} RawCapture;

static _Thread_local RawCapture s_raw_caps[128];
static _Thread_local int s_raw_cap_count = 0;

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
    zobrist_init();
    memset(game, 0, sizeof(GameState));
    game->mode = mode;
    game->human_player = human_player;
    game->white_engine = white_engine;
    game->black_engine = black_engine;
    game->current_player = PLAYER_WHITE;
    game->selected_row = -1;
    game->selected_col = -1;
    game->is_game_over = false;
    game->is_draw = false;
    game->winner = PLAYER_WHITE;
    
    // Initial Italian Checkers 128-bit bitboard setup:
    // White pieces at rows 0, 1, 2 (sq 0..11 -> bits 0..11)
    game->board.white_men = 0x00000FFFU;
    game->board.white_kings = 0;
    // Black pieces at rows 5, 6, 7 (sq 20..31 -> bits 20..31)
    game->board.black_men = 0xFFF00000U;
    game->board.black_kings = 0;

    // Compute initial 64-bit Zobrist hash
    game->hash = zobrist_compute_hash(&game->board, game->current_player);

    // Record initial board position in history
    game->history_count = 0;
    game->history[game->history_count++] = (PositionKey){
        .wm = game->board.white_men,
        .wk = game->board.white_kings,
        .bm = game->board.black_men,
        .bk = game->board.black_kings,
        .player = (uint8_t)game->current_player,
        .hash = game->hash
    };
}


// Forward declarations for DFS functions
static void dfs_white_king_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask, uint8_t is_prom);

static void dfs_black_king_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask, uint8_t is_prom);

// White Pawn Capture DFS
static void dfs_white_pawn_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask) {
    bool has_subjump = false;
    
    // If piece reached promotion rank (row 7 -> sq >= 28), it promotes to Dama (FID Art. 5.7)
    if (curr_sq >= 28 && jumps > 0) {
        dfs_white_king_capture(start_sq, curr_sq, cap_mask, jumps, path, caps,
                               king_caps, first_king_step, b, free_mask, 1);
        return;
    }

    // White pawn moves forward: dir 0 (Up-Right), dir 1 (Up-Left)
    // Rule: White Pawns can ONLY capture Black Pawns (b->black_men)
    for (int d = 0; d < 2; d++) {
        int mid = s_jump_mid[curr_sq][d];
        int dest = s_jump_dest[curr_sq][d];
        if (dest < 0) continue;
        
        if (!((free_mask & (1U << dest)) || dest == start_sq)) continue;
        if (cap_mask & (1U << dest)) continue;
        
        if ((b->black_men & (1U << mid)) && !(cap_mask & (1U << mid))) {
            has_subjump = true;
            path[jumps + 1] = (uint8_t)dest;
            caps[jumps] = (uint8_t)mid;
            
            dfs_white_pawn_capture(start_sq, dest, cap_mask | (1U << mid),
                                   jumps + 1, path, caps,
                                   king_caps, first_king_step, b, free_mask);
        }
    }
    
    if (!has_subjump && jumps > 0) {
        if (s_raw_cap_count < 128) {
            RawCapture *rc = &s_raw_caps[s_raw_cap_count++];
            rc->from = (uint8_t)start_sq;
            rc->to = (uint8_t)curr_sq;
            rc->piece_type = 0;
            rc->is_prom = (curr_sq >= 28) ? 1 : 0;
            rc->jumps = jumps;
            rc->cap_mask = cap_mask;
            rc->king_caps = king_caps;
            rc->first_king_step = first_king_step;
            memcpy(rc->path, path, (jumps + 1) * sizeof(uint8_t));
            memcpy(rc->caps, caps, jumps * sizeof(uint8_t));
        }
    }
}

// White King Capture DFS
static void dfs_white_king_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask, uint8_t is_prom) {
    bool has_subjump = false;
    uint32_t enemy_all = b->black_men | b->black_kings;

    for (int d = 0; d < 4; d++) {
        int mid = s_jump_mid[curr_sq][d];
        int dest = s_jump_dest[curr_sq][d];
        if (dest < 0) continue;
        
        if (!((free_mask & (1U << dest)) || dest == start_sq)) continue;
        if (cap_mask & (1U << dest)) continue;
        
        if ((enemy_all & (1U << mid)) && !(cap_mask & (1U << mid))) {
            has_subjump = true;
            path[jumps + 1] = (uint8_t)dest;
            caps[jumps] = (uint8_t)mid;
            
            bool is_enemy_king = (b->black_kings & (1U << mid)) != 0;
            uint8_t next_kc = king_caps + (is_enemy_king ? 1 : 0);
            uint8_t next_fks = first_king_step;
            if (is_enemy_king && next_fks == 99) {
                next_fks = jumps + 1;
            }
            
            dfs_white_king_capture(start_sq, dest, cap_mask | (1U << mid),
                                   jumps + 1, path, caps,
                                   next_kc, next_fks, b, free_mask, is_prom);
        }
    }
    
    if (!has_subjump && jumps > 0) {
        if (s_raw_cap_count < 128) {
            RawCapture *rc = &s_raw_caps[s_raw_cap_count++];
            rc->from = (uint8_t)start_sq;
            rc->to = (uint8_t)curr_sq;
            rc->piece_type = is_prom ? 0 : 1;
            rc->is_prom = is_prom;
            rc->jumps = jumps;
            rc->cap_mask = cap_mask;
            rc->king_caps = king_caps;
            rc->first_king_step = first_king_step;
            memcpy(rc->path, path, (jumps + 1) * sizeof(uint8_t));
            memcpy(rc->caps, caps, jumps * sizeof(uint8_t));
        }
    }
}

// Black Pawn Capture DFS
static void dfs_black_pawn_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask) {
    bool has_subjump = false;
    
    if (curr_sq <= 3 && jumps > 0) {
        dfs_black_king_capture(start_sq, curr_sq, cap_mask, jumps, path, caps,
                               king_caps, first_king_step, b, free_mask, 1);
        return;
    }

    // Black pawn moves down: dir 2 (Down-Right), dir 3 (Down-Left)
    // Rule: Black Pawns can ONLY capture White Pawns (b->white_men)
    for (int d = 2; d < 4; d++) {
        int mid = s_jump_mid[curr_sq][d];
        int dest = s_jump_dest[curr_sq][d];
        if (dest < 0) continue;
        
        if (!((free_mask & (1U << dest)) || dest == start_sq)) continue;
        if (cap_mask & (1U << dest)) continue;
        
        if ((b->white_men & (1U << mid)) && !(cap_mask & (1U << mid))) {
            has_subjump = true;
            path[jumps + 1] = (uint8_t)dest;
            caps[jumps] = (uint8_t)mid;
            
            dfs_black_pawn_capture(start_sq, dest, cap_mask | (1U << mid),
                                   jumps + 1, path, caps,
                                   king_caps, first_king_step, b, free_mask);
        }
    }
    
    if (!has_subjump && jumps > 0) {
        if (s_raw_cap_count < 128) {
            RawCapture *rc = &s_raw_caps[s_raw_cap_count++];
            rc->from = (uint8_t)start_sq;
            rc->to = (uint8_t)curr_sq;
            rc->piece_type = 0;
            rc->is_prom = (curr_sq <= 3) ? 1 : 0;
            rc->jumps = jumps;
            rc->cap_mask = cap_mask;
            rc->king_caps = king_caps;
            rc->first_king_step = first_king_step;
            memcpy(rc->path, path, (jumps + 1) * sizeof(uint8_t));
            memcpy(rc->caps, caps, jumps * sizeof(uint8_t));
        }
    }
}

// Black King Capture DFS
static void dfs_black_king_capture(int start_sq, int curr_sq, uint32_t cap_mask,
                                   uint8_t jumps, uint8_t *path, uint8_t *caps,
                                   uint8_t king_caps, uint8_t first_king_step,
                                   const Board *b, uint32_t free_mask, uint8_t is_prom) {
    bool has_subjump = false;
    uint32_t enemy_all = b->white_men | b->white_kings;

    for (int d = 0; d < 4; d++) {
        int mid = s_jump_mid[curr_sq][d];
        int dest = s_jump_dest[curr_sq][d];
        if (dest < 0) continue;
        
        if (!((free_mask & (1U << dest)) || dest == start_sq)) continue;
        if (cap_mask & (1U << dest)) continue;
        
        if ((enemy_all & (1U << mid)) && !(cap_mask & (1U << mid))) {
            has_subjump = true;
            path[jumps + 1] = (uint8_t)dest;
            caps[jumps] = (uint8_t)mid;
            
            bool is_enemy_king = (b->white_kings & (1U << mid)) != 0;
            uint8_t next_kc = king_caps + (is_enemy_king ? 1 : 0);
            uint8_t next_fks = first_king_step;
            if (is_enemy_king && next_fks == 99) {
                next_fks = jumps + 1;
            }
            
            dfs_black_king_capture(start_sq, dest, cap_mask | (1U << mid),
                                   jumps + 1, path, caps,
                                   next_kc, next_fks, b, free_mask, is_prom);
        }
    }
    
    if (!has_subjump && jumps > 0) {
        if (s_raw_cap_count < 128) {
            RawCapture *rc = &s_raw_caps[s_raw_cap_count++];
            rc->from = (uint8_t)start_sq;
            rc->to = (uint8_t)curr_sq;
            rc->piece_type = is_prom ? 0 : 1;
            rc->is_prom = is_prom;
            rc->jumps = jumps;
            rc->cap_mask = cap_mask;
            rc->king_caps = king_caps;
            rc->first_king_step = first_king_step;
            memcpy(rc->path, path, (jumps + 1) * sizeof(uint8_t));
            memcpy(rc->caps, caps, jumps * sizeof(uint8_t));
        }
    }
}

const MoveList* game_get_valid_moves(const GameState *game) {
    init_tables();
    g_move_list.count = 0;
    if (!game || game->is_game_over) return &g_move_list;

    const Board *b = &game->board;
    Player player = game->current_player;
    uint32_t occ = BOARD_OCCUPIED(*b);
    uint32_t free_mask = ~occ;

    s_raw_cap_count = 0;

    if (player == PLAYER_WHITE) {
        uint32_t my_men = b->white_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1;
            uint8_t path[8] = { (uint8_t)sq };
            uint8_t caps[8] = { 0 };
            dfs_white_pawn_capture(sq, sq, 0, 0, path, caps, 0, 99, b, free_mask);
        }

        uint32_t my_kings = b->white_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            uint8_t path[8] = { (uint8_t)sq };
            uint8_t caps[8] = { 0 };
            dfs_white_king_capture(sq, sq, 0, 0, path, caps, 0, 99, b, free_mask, 0);
        }
    } else {
        uint32_t my_men = b->black_men;
        while (my_men) {
            int sq = __builtin_ctz(my_men);
            my_men &= my_men - 1;
            uint8_t path[8] = { (uint8_t)sq };
            uint8_t caps[8] = { 0 };
            dfs_black_pawn_capture(sq, sq, 0, 0, path, caps, 0, 99, b, free_mask);
        }

        uint32_t my_kings = b->black_kings;
        while (my_kings) {
            int sq = __builtin_ctz(my_kings);
            my_kings &= my_kings - 1;
            uint8_t path[8] = { (uint8_t)sq };
            uint8_t caps[8] = { 0 };
            dfs_black_king_capture(sq, sq, 0, 0, path, caps, 0, 99, b, free_mask, 0);
        }
    }

    // Apply FID 4-Tier Law of Maximum Capture Filtering
    if (s_raw_cap_count > 0) {
        // Tier 1: Maximum number of captured pieces
        uint8_t max_jumps = 0;
        for (int i = 0; i < s_raw_cap_count; i++) {
            if (s_raw_caps[i].jumps > max_jumps) max_jumps = s_raw_caps[i].jumps;
        }

        bool keep[128];
        for (int i = 0; i < s_raw_cap_count; i++) {
            keep[i] = (s_raw_caps[i].jumps == max_jumps);
        }

        // Tier 2: Precedence to Dama (King) at equal number of pieces
        bool has_dama = false;
        for (int i = 0; i < s_raw_cap_count; i++) {
            if (keep[i] && s_raw_caps[i].piece_type == 1) {
                has_dama = true;
                break;
            }
        }
        if (has_dama) {
            for (int i = 0; i < s_raw_cap_count; i++) {
                if (keep[i] && s_raw_caps[i].piece_type != 1) {
                    keep[i] = false;
                }
            }
        }

        // Tier 3: Maximum number of Kings captured (for King moves)
        if (has_dama) {
            uint8_t max_kings = 0;
            for (int i = 0; i < s_raw_cap_count; i++) {
                if (keep[i] && s_raw_caps[i].king_caps > max_kings) {
                    max_kings = s_raw_caps[i].king_caps;
                }
            }
            for (int i = 0; i < s_raw_cap_count; i++) {
                if (keep[i] && s_raw_caps[i].king_caps < max_kings) {
                    keep[i] = false;
                }
            }

            // Tier 4: King captured earlier (first_king_step)
            if (max_kings > 0) {
                uint8_t min_step = 99;
                for (int i = 0; i < s_raw_cap_count; i++) {
                    if (keep[i] && s_raw_caps[i].first_king_step < min_step) {
                        min_step = s_raw_caps[i].first_king_step;
                    }
                }
                for (int i = 0; i < s_raw_cap_count; i++) {
                    if (keep[i] && s_raw_caps[i].first_king_step > min_step) {
                        keep[i] = false;
                    }
                }
            }
        }

        // Deduplicate and populate g_move_list
        for (int i = 0; i < s_raw_cap_count; i++) {
            if (!keep[i]) continue;

            Move m;
            m.from = s_raw_caps[i].from;
            m.to = s_raw_caps[i].to;
            m.piece_type = s_raw_caps[i].piece_type;
            m.is_prom = s_raw_caps[i].is_prom;
            m.is_cap = 1;
            m.jumps = s_raw_caps[i].jumps;
            m.cap_mask = s_raw_caps[i].cap_mask;
            memset(m.path, 0, sizeof(m.path));
            memset(m.caps, 0, sizeof(m.caps));
            memcpy(m.path, s_raw_caps[i].path, (m.jumps + 1) * sizeof(uint8_t));
            memcpy(m.caps, s_raw_caps[i].caps, m.jumps * sizeof(uint8_t));

            bool dup = false;
            for (int j = 0; j < g_move_list.count; j++) {
                if (move_equals(g_move_list.moves[j], m)) {
                    dup = true;
                    break;
                }
            }
            if (!dup && g_move_list.count < 48) {
                g_move_list.moves[g_move_list.count++] = m;
            }
        }

        return &g_move_list;
    }

    // No captures -> Generate simple quiet moves
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
                    Move m;
                    m.from = (uint8_t)sq;
                    m.to = (uint8_t)dest;
                    m.piece_type = 0;
                    m.is_prom = is_prom ? 1 : 0;
                    m.is_cap = 0;
                    m.jumps = 0;
                    m.cap_mask = 0;
                    m.path[0] = (uint8_t)sq;
                    m.path[1] = (uint8_t)dest;
                    memset(m.caps, 0, sizeof(m.caps));
                    if (g_move_list.count < 48) {
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
                    Move m;
                    m.from = (uint8_t)sq;
                    m.to = (uint8_t)dest;
                    m.piece_type = 1;
                    m.is_prom = 0;
                    m.is_cap = 0;
                    m.jumps = 0;
                    m.cap_mask = 0;
                    m.path[0] = (uint8_t)sq;
                    m.path[1] = (uint8_t)dest;
                    memset(m.caps, 0, sizeof(m.caps));
                    if (g_move_list.count < 48) {
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
                    Move m;
                    m.from = (uint8_t)sq;
                    m.to = (uint8_t)dest;
                    m.piece_type = 0;
                    m.is_prom = is_prom ? 1 : 0;
                    m.is_cap = 0;
                    m.jumps = 0;
                    m.cap_mask = 0;
                    m.path[0] = (uint8_t)sq;
                    m.path[1] = (uint8_t)dest;
                    memset(m.caps, 0, sizeof(m.caps));
                    if (g_move_list.count < 48) {
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
                    Move m;
                    m.from = (uint8_t)sq;
                    m.to = (uint8_t)dest;
                    m.piece_type = 1;
                    m.is_prom = 0;
                    m.is_cap = 0;
                    m.jumps = 0;
                    m.cap_mask = 0;
                    m.path[0] = (uint8_t)sq;
                    m.path[1] = (uint8_t)dest;
                    memset(m.caps, 0, sizeof(m.caps));
                    if (g_move_list.count < 48) {
                        g_move_list.moves[g_move_list.count++] = m;
                    }
                }
            }
        }
    }

    return &g_move_list;
}

int game_get_repetition_count(const GameState *game) {
    if (!game || game->history_count == 0) return 0;
    
    int count = 0;
    uint64_t target_hash = game->hash;

    for (int i = 0; i < game->history_count; i++) {
        if (game->history[i].hash == target_hash) {
            count++;
        }
    }
    return count;
}

bool game_is_threefold_repetition(const GameState *game) {
    return game_get_repetition_count(game) >= 3;
}

bool game_execute_move(GameState *game, Move move) {
    if (move_is_none(move)) return false;
    
    int from = move.from;
    int to = move.to;
    uint32_t from_mask = 1U << from;
    uint32_t to_mask = 1U << to;
    Board *b = &game->board;
    Player cur = game->current_player;
    
    if (cur == PLAYER_WHITE) {
        if (b->white_men & from_mask) {
            b->white_men &= ~from_mask;
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_MAN][from];
            if (move.is_prom || to >= 28) {
                b->white_kings |= to_mask;
                game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_KING][to];
            } else {
                b->white_men |= to_mask;
                game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_MAN][to];
            }
        } else if (b->white_kings & from_mask) {
            b->white_kings &= ~from_mask;
            b->white_kings |= to_mask;
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_KING][from];
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_KING][to];
        } else {
            return false;
        }
        
        if (move.is_cap) {
            for (int j = 0; j < move.jumps; j++) {
                int cap_sq = move.caps[j];
                uint32_t cmask = 1U << cap_sq;
                PieceType cap_type = PIECE_NONE;
                if (b->black_men & cmask) {
                    b->black_men &= ~cmask;
                    cap_type = PIECE_BLACK_PAWN;
                    game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_MAN][cap_sq];
                } else if (b->black_kings & cmask) {
                    b->black_kings &= ~cmask;
                    cap_type = PIECE_BLACK_DAMA;
                    game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_KING][cap_sq];
                }
                if (game->white_eaten_count < 12 && cap_type != PIECE_NONE) {
                    game->white_eaten_list[game->white_eaten_count++] = cap_type;
                }
            }
        }
    } else {
        if (b->black_men & from_mask) {
            b->black_men &= ~from_mask;
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_MAN][from];
            if (move.is_prom || to <= 3) {
                b->black_kings |= to_mask;
                game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_KING][to];
            } else {
                b->black_men |= to_mask;
                game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_MAN][to];
            }
        } else if (b->black_kings & from_mask) {
            b->black_kings &= ~from_mask;
            b->black_kings |= to_mask;
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_KING][from];
            game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_KING][to];
        } else {
            return false;
        }
        
        if (move.is_cap) {
            for (int j = 0; j < move.jumps; j++) {
                int cap_sq = move.caps[j];
                uint32_t cmask = 1U << cap_sq;
                PieceType cap_type = PIECE_NONE;
                if (b->white_men & cmask) {
                    b->white_men &= ~cmask;
                    cap_type = PIECE_WHITE_PAWN;
                    game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_MAN][cap_sq];
                } else if (b->white_kings & cmask) {
                    b->white_kings &= ~cmask;
                    cap_type = PIECE_WHITE_DAMA;
                    game->hash ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_KING][cap_sq];
                }
                if (game->black_eaten_count < 12 && cap_type != PIECE_NONE) {
                    game->black_eaten_list[game->black_eaten_count++] = cap_type;
                }
            }
        }
    }
    
    // Switch turn
    game->hash ^= g_zobrist_player;
    game->current_player = (cur == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Record new state in history
    if (game->history_count < MAX_GAME_HISTORY) {
        game->history[game->history_count++] = (PositionKey){
            .wm = game->board.white_men,
            .wk = game->board.white_kings,
            .bm = game->board.black_men,
            .bk = game->board.black_kings,
            .player = (uint8_t)game->current_player,
            .hash = game->hash
        };
    }
    
    // Check threefold repetition (Patta per ripetizione di posizione)
    if (game_is_threefold_repetition(game)) {
        game->is_game_over = true;
        game->is_draw = true;
        return true;
    }
    
    // Check if next player has valid moves
    const MoveList *next_moves = game_get_valid_moves(game);
    if (next_moves->count == 0) {
        game->is_game_over = true;
        game->is_draw = false;
        game->winner = cur; // Current player won!
    }
    
    return true;
}


