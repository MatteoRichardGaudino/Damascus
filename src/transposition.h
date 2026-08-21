#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TT_DEFAULT_SIZE (1U << 19) // 524,288 entries (~8.4 MB)
#define TT_NO_NODE      UINT32_MAX

typedef struct {
    uint64_t key;      // 64-bit Zobrist key
    uint32_t node_idx; // MCTS node index in the pool
    uint16_t depth;    // Search depth / tree ply
    uint16_t age;      // Search epoch / move counter
} TTEntry;

typedef struct {
    TTEntry *entries;
    uint32_t size;     // Must be power of 2
    uint32_t mask;     // size - 1
    uint32_t count;    // Occupied entries count
    uint32_t hits;     // Successful lookups
    uint32_t lookups;  // Total lookup attempts
} TranspositionTable;

// Lifecycle
bool tt_init(TranspositionTable *tt, uint32_t num_entries);
void tt_clear(TranspositionTable *tt);
void tt_free(TranspositionTable *tt);

// Query & Store
void tt_store(TranspositionTable *tt, uint64_t key, uint32_t node_idx, uint16_t depth, uint16_t age);
uint32_t tt_probe(TranspositionTable *tt, uint64_t key);

// Statistics
float tt_get_hit_rate(const TranspositionTable *tt);

#endif // TRANSPOSITION_H
