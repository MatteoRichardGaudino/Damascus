/*______________________________________________________________________________
  Damascus - Unified Endgame Database Subsystem (WLD - Win/Loss/Draw)
  Official Kingsrow 8-Piece Database Integration (<= 8 pieces via egdb64)
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
    WLD_BACKEND_OFFICIAL_8PIECE = 1 // data/wld/*.cpr1 (<= 8 pieces via egdb64)
} WLDBackendType;

typedef struct {
    bool available;
    WLDBackendType active_backend;
    int max_pieces;
    size_t loaded_slices;
    char status_message[128];
} WLDStatus;

#define WLD_OFFICIAL_DB_DIR "data/wld"

// Lifecycle & Backend Management
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

// Legacy backwards-compatibility wrappers for existing engine modules
void wld_db_init(void);
WLDValue wld_db_probe(const GameState *game);

static inline bool wld_db_is_endgame(const Board *b) {
    int count = __builtin_popcount(b->white_men) + 
                __builtin_popcount(b->white_kings) + 
                __builtin_popcount(b->black_men) + 
                __builtin_popcount(b->black_kings);
    return count <= 8;
}

#ifdef __cplusplus
}
#endif

#endif // WLD_DB_H
