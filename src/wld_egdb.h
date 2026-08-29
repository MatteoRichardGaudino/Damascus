/*______________________________________________________________________________
  Damascus - Official 8-Piece Endgame Database Driver (Kingsrow EGDB)
  Interfaces with egdb64.dll and validates data/wld tablebase slices
______________________________________________________________________________*/

#ifndef WLD_EGDB_H
#define WLD_EGDB_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration of WLDValue (defined in wld_db.h)
typedef enum WLDValue WLDValue;

// Ed Gilbert's EGDB driver position representation (32-bit bitboards)
typedef struct {
    uint64_t black;
    uint64_t white;
    uint64_t king;
} EGDB_POSITION;

typedef struct EGDB_DRIVER EGDB_DRIVER;

typedef struct EGDB_DRIVER {
    int (*lookup)(EGDB_DRIVER *handle, EGDB_POSITION *pos, int color, int cl);
    int (*reset_stats)(EGDB_DRIVER *handle);
    int (*print_stats)(EGDB_DRIVER *handle);
    int (*verify)(EGDB_DRIVER *handle);
    int (*close)(EGDB_DRIVER *handle);
    void *internal_data;
} EGDB_DRIVER;

// Check if EGDB dynamic driver is supported on current OS / platform
bool wld_egdb_is_supported(void);

// Scan directory for valid slice files (returns total valid slices count, e.g. 90)
size_t wld_egdb_scan_slices(const char *db_dir, size_t *out_total_bytes, size_t *out_missing_slices);

// Initialize official EGDB tablebase driver
bool wld_egdb_init(const char *db_dir, int cache_mb);

// Close and release EGDB driver resources
void wld_egdb_close(void);

// Check if EGDB tablebase is currently loaded and ready
bool wld_egdb_is_ready(void);

// Probe endgame position (GameState or CompactState with <= 8 pieces)
// Returns WLD_UNKNOWN if not in tablebase or driver not initialized
WLDValue wld_egdb_probe(const GameState *game);
WLDValue wld_egdb_probe_compact(const CompactState *state);

// Direct lookup on raw EGDB_POSITION
int wld_egdb_lookup_raw(EGDB_POSITION *pos, int color, int cl);

// Get number of loaded slices
size_t wld_egdb_get_loaded_slices(void);

// Get max pieces supported by current loaded EGDB database (typically 8)
int wld_egdb_get_max_pieces(void);

#ifdef __cplusplus
}
#endif

#endif // WLD_EGDB_H
