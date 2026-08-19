#ifndef WLD_DB_H
#define WLD_DB_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WLD_UNKNOWN = 0,
    WLD_WIN_WHITE = 1,
    WLD_WIN_BLACK = 2,
    WLD_DRAW = 3
} WLDValue;

#define WLD_DB_DEFAULT_PATH "data/damascus_wld.bin"

// Initialize the native WLD tablebase (loads from binary if present, otherwise solves and saves)
void wld_db_init(void);

// Query if a position is in the WLD database (<= 5 pieces)
// Returns WLD_UNKNOWN if not in tablebase (> 5 pieces)
WLDValue wld_db_probe(const GameState *game);

// Save and Load binary tablebase
bool wld_db_save(const char *filepath);
bool wld_db_load(const char *filepath);

// Helper to check if piece count is within tablebase range
static inline bool wld_db_is_endgame(const Board *b) {
    int count = __builtin_popcount(b->white_men) + 
                __builtin_popcount(b->white_kings) + 
                __builtin_popcount(b->black_men) + 
                __builtin_popcount(b->black_kings);
    return count <= 5;
}

#endif // WLD_DB_H
