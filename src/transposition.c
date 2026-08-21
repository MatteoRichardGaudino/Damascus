#include "transposition.h"
#include <stdlib.h>
#include <string.h>

bool tt_init(TranspositionTable *tt, uint32_t num_entries) {
    if (!tt) return false;
    
    // Ensure power of 2
    uint32_t size = 1;
    while (size < num_entries) {
        size <<= 1;
    }

    tt->entries = (TTEntry*)calloc(size, sizeof(TTEntry));
    if (!tt->entries) {
        tt->size = 0;
        tt->mask = 0;
        tt->count = 0;
        tt->hits = 0;
        tt->lookups = 0;
        return false;
    }

    tt->size = size;
    tt->mask = size - 1;
    tt->count = 0;
    tt->hits = 0;
    tt->lookups = 0;

    for (uint32_t i = 0; i < size; i++) {
        tt->entries[i].node_idx = TT_NO_NODE;
    }

    return true;
}

void tt_clear(TranspositionTable *tt) {
    if (!tt || !tt->entries) return;
    for (uint32_t i = 0; i < tt->size; i++) {
        tt->entries[i].key = 0;
        tt->entries[i].node_idx = TT_NO_NODE;
        tt->entries[i].depth = 0;
        tt->entries[i].age = 0;
    }
    tt->count = 0;
    tt->hits = 0;
    tt->lookups = 0;
}

void tt_free(TranspositionTable *tt) {
    if (tt && tt->entries) {
        free(tt->entries);
        tt->entries = NULL;
        tt->size = 0;
        tt->mask = 0;
        tt->count = 0;
        tt->hits = 0;
        tt->lookups = 0;
    }
}

void tt_store(TranspositionTable *tt, uint64_t key, uint32_t node_idx, uint16_t depth, uint16_t age) {
    if (!tt || !tt->entries || node_idx == TT_NO_NODE) return;

    uint32_t idx = (uint32_t)(key & tt->mask);
    TTEntry *entry = &tt->entries[idx];

    if (entry->node_idx == TT_NO_NODE) {
        tt->count++;
    } else if (entry->key != key) {
        // Replacement policy: prefer deeper or newer entries
        if (entry->age < age || (entry->age == age && depth >= entry->depth)) {
            // Replace old/shallower entry
        } else {
            // Keep existing entry
            return;
        }
    }

    entry->key = key;
    entry->node_idx = node_idx;
    entry->depth = depth;
    entry->age = age;
}

uint32_t tt_probe(TranspositionTable *tt, uint64_t key) {
    if (!tt || !tt->entries) return TT_NO_NODE;

    tt->lookups++;
    uint32_t idx = (uint32_t)(key & tt->mask);
    TTEntry *entry = &tt->entries[idx];

    if (entry->node_idx != TT_NO_NODE && entry->key == key) {
        tt->hits++;
        return entry->node_idx;
    }

    return TT_NO_NODE;
}

float tt_get_hit_rate(const TranspositionTable *tt) {
    if (!tt || tt->lookups == 0) return 0.0f;
    return (float)tt->hits * 100.0f / (float)tt->lookups;
}
