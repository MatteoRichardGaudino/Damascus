#include "zobrist.h"

uint64_t g_zobrist_pieces[ZOBRIST_PIECE_COUNT][ZOBRIST_SQUARE_COUNT];
uint64_t g_zobrist_player;
static bool s_zobrist_initialized = false;

// SplitMix64 pseudo-random generator with fixed seed
static inline uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void zobrist_init(void) {
    if (s_zobrist_initialized) return;

    uint64_t rng_state = 0xDA3A5C051A11A415ULL; // Deterministic seed

    for (int pt = 0; pt < ZOBRIST_PIECE_COUNT; pt++) {
        for (int sq = 0; sq < ZOBRIST_SQUARE_COUNT; sq++) {
            g_zobrist_pieces[pt][sq] = splitmix64(&rng_state);
        }
    }

    g_zobrist_player = splitmix64(&rng_state);
    s_zobrist_initialized = true;
}

uint64_t zobrist_compute_hash(const Board *board, Player player) {
    if (!s_zobrist_initialized) {
        zobrist_init();
    }

    uint64_t h = 0;

    // White Men
    uint32_t wm = board->white_men;
    while (wm) {
        int sq = __builtin_ctz(wm);
        wm &= wm - 1;
        h ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_MAN][sq];
    }

    // White Kings
    uint32_t wk = board->white_kings;
    while (wk) {
        int sq = __builtin_ctz(wk);
        wk &= wk - 1;
        h ^= g_zobrist_pieces[ZOBRIST_PIECE_WHITE_KING][sq];
    }

    // Black Men
    uint32_t bm = board->black_men;
    while (bm) {
        int sq = __builtin_ctz(bm);
        bm &= bm - 1;
        h ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_MAN][sq];
    }

    // Black Kings
    uint32_t bk = board->black_kings;
    while (bk) {
        int sq = __builtin_ctz(bk);
        bk &= bk - 1;
        h ^= g_zobrist_pieces[ZOBRIST_PIECE_BLACK_KING][sq];
    }

    // Active Player (toggle if Black)
    if (player == PLAYER_BLACK) {
        h ^= g_zobrist_player;
    }

    return h;
}
