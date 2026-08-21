#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>

// Piece types for Zobrist index
#define ZOBRIST_PIECE_WHITE_MAN  0
#define ZOBRIST_PIECE_WHITE_KING 1
#define ZOBRIST_PIECE_BLACK_MAN  2
#define ZOBRIST_PIECE_BLACK_KING 3
#define ZOBRIST_PIECE_COUNT      4
#define ZOBRIST_SQUARE_COUNT     32

// Global deterministic random bitstrings
extern uint64_t g_zobrist_pieces[ZOBRIST_PIECE_COUNT][ZOBRIST_SQUARE_COUNT];
extern uint64_t g_zobrist_player; // XOR toggle when it is Black's turn

// Initialize the Zobrist hash tables using deterministic PRNG (SplitMix64)
void zobrist_init(void);

// Compute 64-bit Zobrist hash from scratch for a given board position and active player
uint64_t zobrist_compute_hash(const Board *board, Player player);

#endif // ZOBRIST_H
