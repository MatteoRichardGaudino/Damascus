/*______________________________________________________________________________
  Damascus - Unified Endgame Database Subsystem (WLD - Win/Loss/Draw)
  Multi-backend support: Native Reduced DB (<= 4 pieces) & Official Kingsrow (<= 8 pieces)
______________________________________________________________________________*/

#ifndef WLD_DB_H
#define WLD_DB_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WLDValue {
    WLD_UNKNOWN = 0,
    WLD_WIN_WHITE = 1,
    WLD_WIN_BLACK = 2,
    WLD_DRAW = 3
} WLDValue;

typedef enum {
    WLD_BACKEND_NONE = 0,
    WLD_BACKEND_REDUCED_NATIVE = 1, // data/damascus_wld.bin (<= 4 pieces)
    WLD_BACKEND_OFFICIAL_8PIECE = 2 // data/wld/*.cpr1 (<= 8 pieces via egdb64)
} WLDBackendType;

typedef struct {
    bool available;
    WLDBackendType active_backend;
    int max_pieces;
    size_t loaded_slices;
    char status_message[128];
} WLDStatus;

#define WLD_DB_DEFAULT_PATH "data/damascus_wld.bin"
#define WLD_OFFICIAL_DB_DIR "data/wld"

// Lifecycle & Multi-Backend Management
bool wld_init_backend(WLDBackendType backend);
void wld_cleanup(void);
WLDStatus wld_get_status(WLDBackendType backend);
WLDBackendType wld_get_active_backend(void);

// Backend Names, Parsing & Custom Paths
const char *wld_backend_get_name(WLDBackendType backend);
const char *wld_backend_get_cli_name(WLDBackendType backend);
WLDBackendType wld_backend_parse(const char *name);
void wld_set_custom_path(const char *path);
const char *wld_get_custom_path(void);

// Unified Probing API
WLDValue wld_probe_state(const GameState *game);
bool wld_is_endgame_state(const GameState *game);

// Native tablebase direct persistence
bool wld_db_save(const char *filepath);
bool wld_db_load(const char *filepath);

// Legacy backwards-compatibility wrappers for existing engine modules
void wld_db_init(void);
WLDValue wld_db_probe(const GameState *game);

static inline bool wld_db_is_endgame(const Board *b) {
    int count = __builtin_popcount(b->white_men) + 
                __builtin_popcount(b->white_kings) + 
                __builtin_popcount(b->black_men) + 
                __builtin_popcount(b->black_kings);
    return count <= 4;
}

#ifdef __cplusplus
}
#endif

#endif // WLD_DB_H
