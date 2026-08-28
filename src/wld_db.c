/*______________________________________________________________________________
  Damascus Endgame Database Subsystem (WLD - Win/Loss/Draw)
  Official Kingsrow 8-Piece DB (data/wld, <= 8 pieces via egdb64 driver)
______________________________________________________________________________*/

#include "wld_db.h"
#include "wld_egdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WLDBackendType s_active_backend = WLD_BACKEND_NONE;
static char s_custom_wld_path[256] = "";

// Backend Names, Parsing & Custom Paths
const char *wld_backend_get_name(WLDBackendType backend) {
    switch (backend) {
        case WLD_BACKEND_OFFICIAL_8PIECE:
            return "UFFICIALE 8 PEZZI (data/wld)";
        case WLD_BACKEND_NONE:
        default:
            return "DISATTIVATO";
    }
}

const char *wld_backend_get_cli_name(WLDBackendType backend) {
    switch (backend) {
        case WLD_BACKEND_OFFICIAL_8PIECE:
            return "official";
        case WLD_BACKEND_NONE:
        default:
            return "none";
    }
}

WLDBackendType wld_backend_parse(const char *name) {
    if (!name || name[0] == '\0') return WLD_BACKEND_NONE;
#ifdef _WIN32
    #define WLD_STRCASECMP _stricmp
#else
    #define WLD_STRCASECMP strcasecmp
#endif
    if (WLD_STRCASECMP(name, "official") == 0 ||
        WLD_STRCASECMP(name, "8piece") == 0 ||
        WLD_STRCASECMP(name, "8p") == 0 ||
        WLD_STRCASECMP(name, "kingsrow") == 0 ||
        WLD_STRCASECMP(name, "kr") == 0) {
        return WLD_BACKEND_OFFICIAL_8PIECE;
    }
    return WLD_BACKEND_NONE;
}

void wld_set_custom_path(const char *path) {
    if (path) {
        snprintf(s_custom_wld_path, sizeof(s_custom_wld_path), "%s", path);
    } else {
        s_custom_wld_path[0] = '\0';
    }
}

const char *wld_get_custom_path(void) {
    return s_custom_wld_path;
}

// Unified Backend Management
bool wld_init_backend(WLDBackendType backend) {
    const char *db_dir = s_custom_wld_path[0] ? s_custom_wld_path : WLD_OFFICIAL_DB_DIR;

    if (backend == WLD_BACKEND_OFFICIAL_8PIECE) {
        if (!wld_egdb_is_supported()) {
            fprintf(stderr, "[WLD ERROR] Official 8-Piece DB is not supported on this platform.\n");
            s_active_backend = WLD_BACKEND_NONE;
            return false;
        }
        if (wld_egdb_init(db_dir, 128)) {
            s_active_backend = WLD_BACKEND_OFFICIAL_8PIECE;
            return true;
        } else {
            fprintf(stderr, "[WLD ERROR] Failed to load Official 8-Piece DB from path: %s\n", db_dir);
            s_active_backend = WLD_BACKEND_NONE;
            return false;
        }
    }

    s_active_backend = WLD_BACKEND_NONE;
    return true;
}

void wld_cleanup(void) {
    if (wld_egdb_is_ready()) {
        wld_egdb_close();
    }
    s_active_backend = WLD_BACKEND_NONE;
}

WLDStatus wld_get_status(WLDBackendType backend) {
    WLDStatus status;
    memset(&status, 0, sizeof(status));
    status.active_backend = s_active_backend;

    if (backend == WLD_BACKEND_OFFICIAL_8PIECE) {
        if (!wld_egdb_is_supported()) {
            status.available = false;
            status.max_pieces = 0;
            status.loaded_slices = 0;
            snprintf(status.status_message, sizeof(status.status_message),
                     "[NON SUPPORTATO SU QUESTA PIATTAFORMA]");
            return status;
        }

        const char *scan_dir = s_custom_wld_path[0] ? s_custom_wld_path : WLD_OFFICIAL_DB_DIR;
        size_t missing = 0;
        size_t total_bytes = 0;
        size_t found = wld_egdb_scan_slices(scan_dir, &total_bytes, &missing);
        if (found > 0 && missing == 0) {
            status.available = true;
            status.max_pieces = 8;
            status.loaded_slices = found;
            snprintf(status.status_message, sizeof(status.status_message),
                     "[OK: %zu SLICE CARICATE (8 PEZZI)]", found);
        } else {
            status.available = false;
            status.max_pieces = 0;
            status.loaded_slices = found;
            snprintf(status.status_message, sizeof(status.status_message),
                     "[FILE MANCANTI - DB DISABILITATO]");
        }
        return status;
    } else {
        status.available = true;
        status.max_pieces = 0;
        status.loaded_slices = 0;
        snprintf(status.status_message, sizeof(status.status_message),
                 "[TABELLE FINALI DISATTIVATE]");
        return status;
    }
}

WLDBackendType wld_get_active_backend(void) {
    return s_active_backend;
}

WLDValue wld_probe_state(const GameState *game) {
    if (!game || s_active_backend != WLD_BACKEND_OFFICIAL_8PIECE) return WLD_UNKNOWN;
    return wld_egdb_probe(game);
}

bool wld_is_endgame_state(const GameState *game) {
    if (!game || s_active_backend != WLD_BACKEND_OFFICIAL_8PIECE) return false;
    int count = __builtin_popcount(game->board.white_men) +
                __builtin_popcount(game->board.white_kings) +
                __builtin_popcount(game->board.black_men) +
                __builtin_popcount(game->board.black_kings);
    return count <= 8;
}

// Legacy Wrappers
void wld_db_init(void) {
    if (s_active_backend != WLD_BACKEND_NONE) return;
    wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
}

WLDValue wld_db_probe(const GameState *game) {
    if (s_active_backend == WLD_BACKEND_NONE) {
        wld_db_init();
    }
    return wld_probe_state(game);
}

