/*______________________________________________________________________________
  Damascus - Official 8-Piece Endgame Database Driver (Kingsrow EGDB)
  Dynamic DLL loader, Filesystem Slice Scanner, and High-Throughput Probing
______________________________________________________________________________*/

#include "wld_egdb.h"
#include "wld_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// Expected slices list in Kingsrow Italian 8-piece database (45 base slice names)
static const char *s_expected_slice_names[] = {
    "db2", "db3", "db4", "db5", "db6", "db7",
    "db8-0404", "db8-0413", "db8-0422", "db8-0431", "db8-0440",
    "db8-0503", "db8-0512", "db8-0521", "db8-0530",
    "db8-1313", "db8-1322", "db8-1331", "db8-1340",
    "db8-1403", "db8-1412", "db8-1421", "db8-1430",
    "db8-2222", "db8-2231", "db8-2240",
    "db8-2303", "db8-2312", "db8-2321", "db8-2330",
    "db8-3131", "db8-3140",
    "db8-3203", "db8-3212", "db8-3221", "db8-3230",
    "db8-4040",
    "db8-4103", "db8-4112", "db8-4121", "db8-4130",
    "db8-5003", "db8-5012", "db8-5021", "db8-5030"
};
#define EXPECTED_SLICE_BASES_COUNT (sizeof(s_expected_slice_names) / sizeof(s_expected_slice_names[0]))
#define EXPECTED_TOTAL_SLICES (EXPECTED_SLICE_BASES_COUNT * 2) // 45 .cpr1 + 45 .idx1 = 90

#ifdef _WIN32
typedef int (*egdb_identify_fn)(const char *path, int *db_type, int *max_pieces);
typedef EGDB_DRIVER* (*egdb_open_fn)(int db_type, int pieces, int cache_mb, const char *directory, void (*msg_fn)(char*));

static HMODULE s_egdb_module = NULL;
static EGDB_DRIVER *s_egdb_handle = NULL;
static egdb_identify_fn s_fn_identify = NULL;
static egdb_open_fn s_fn_open = NULL;
#endif

static bool s_is_ready = false;
static size_t s_loaded_slices = 0;
static int s_max_pieces = 0;

static void egdb_msg_callback(char *msg) {
    (void)msg; // Quiet operation during search
}

bool wld_egdb_is_supported(void) {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

static bool file_exists_and_get_size(const char *filepath, size_t *out_size) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(filepath, GetFileExInfoStandard, &fad)) {
        if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (out_size) {
                ULARGE_INTEGER sz;
                sz.LowPart = fad.nFileSizeLow;
                sz.HighPart = fad.nFileSizeHigh;
                *out_size = (size_t)sz.QuadPart;
            }
            return true;
        }
    }
    return false;
#else
    struct stat st;
    if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
        if (out_size) *out_size = (size_t)st.st_size;
        return true;
    }
    return false;
#endif
}

static size_t scan_slices_in_directory(const char *dir, size_t *out_total_bytes, size_t *out_missing_slices) {
    if (!dir || dir[0] == '\0') return 0;
    size_t found_count = 0;
    size_t missing_count = 0;
    size_t total_bytes = 0;

    for (size_t i = 0; i < EXPECTED_SLICE_BASES_COUNT; i++) {
        char path_cpr[512];
        char path_idx[512];
        snprintf(path_cpr, sizeof(path_cpr), "%s/%s.cpr1", dir, s_expected_slice_names[i]);
        snprintf(path_idx, sizeof(path_idx), "%s/%s.idx1", dir, s_expected_slice_names[i]);

        size_t sz_cpr = 0, sz_idx = 0;
        if (file_exists_and_get_size(path_cpr, &sz_cpr)) {
            found_count++;
            total_bytes += sz_cpr;
        } else {
            missing_count++;
        }

        if (file_exists_and_get_size(path_idx, &sz_idx)) {
            found_count++;
            total_bytes += sz_idx;
        } else {
            missing_count++;
        }
    }

    if (out_total_bytes) *out_total_bytes = total_bytes;
    if (out_missing_slices) *out_missing_slices = missing_count;
    return found_count;
}

size_t wld_egdb_scan_slices(const char *db_dir, size_t *out_total_bytes, size_t *out_missing_slices) {
    if (!db_dir || db_dir[0] == '\0') return 0;

    size_t missing = 0;
    size_t bytes = 0;
    size_t found = scan_slices_in_directory(db_dir, &bytes, &missing);
    if (found > 0 && missing == 0) {
        if (out_total_bytes) *out_total_bytes = bytes;
        if (out_missing_slices) *out_missing_slices = missing;
        return found;
    }

    // Try relative candidates only if the default directory "data/wld" was requested
    if (strcmp(db_dir, "data/wld") == 0) {
        const char *candidates[] = {
            "../data/wld",
            "../../data/wld",
            "third_party/engines/kingsrow_italian/app/db",
            "../third_party/engines/kingsrow_italian/app/db",
            "../../third_party/engines/kingsrow_italian/app/db"
        };

        for (size_t c = 0; c < sizeof(candidates) / sizeof(candidates[0]); c++) {
            size_t c_missing = 0;
            size_t c_bytes = 0;
            size_t c_found = scan_slices_in_directory(candidates[c], &c_bytes, &c_missing);
            if (c_found > 0 && c_missing == 0) {
                if (out_total_bytes) *out_total_bytes = c_bytes;
                if (out_missing_slices) *out_missing_slices = c_missing;
                return c_found;
            }
        }
    }

    if (out_total_bytes) *out_total_bytes = bytes;
    if (out_missing_slices) *out_missing_slices = missing;
    return found;
}

#ifdef _WIN32
static bool find_egdb_dll(char *out_path, size_t max_len) {
    const char *candidates[] = {
        "third_party/engines/kingsrow_italian/app/egdb64.dll",
        "../third_party/engines/kingsrow_italian/app/egdb64.dll",
        "../../third_party/engines/kingsrow_italian/app/egdb64.dll",
        "third_party/engines/checkerboard/app/egdb64.dll",
        "../third_party/engines/checkerboard/app/egdb64.dll",
        "egdb64.dll"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        size_t sz = 0;
        if (file_exists_and_get_size(candidates[i], &sz)) {
            char resolved[512];
            if (GetFullPathNameA(candidates[i], sizeof(resolved), resolved, NULL)) {
                snprintf(out_path, max_len, "%s", resolved);
            } else {
                snprintf(out_path, max_len, "%s", candidates[i]);
            }
            return true;
        }
    }
    return false;
}

static bool resolve_db_dir(const char *requested_dir, char *out_dir, size_t max_len) {
    const char *candidates[] = {
        requested_dir,
        "data/wld",
        "../data/wld",
        "../../data/wld",
        "third_party/engines/kingsrow_italian/app/db",
        "../third_party/engines/kingsrow_italian/app/db",
        "../../third_party/engines/kingsrow_italian/app/db"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (!candidates[i] || candidates[i][0] == '\0') continue;
        size_t missing = 0;
        size_t found = wld_egdb_scan_slices(candidates[i], NULL, &missing);
        if (found > 0 && missing == 0) {
            char resolved[512];
            if (GetFullPathNameA(candidates[i], sizeof(resolved), resolved, NULL)) {
                snprintf(out_dir, max_len, "%s", resolved);
            } else {
                snprintf(out_dir, max_len, "%s", candidates[i]);
            }
            return true;
        }
    }
    return false;
}
#endif

bool wld_egdb_init(const char *db_dir, int cache_mb) {
#ifndef _WIN32
    (void)db_dir; (void)cache_mb;
    return false;
#else
    if (s_is_ready) return true;

    char dll_path[512] = "";
    if (!find_egdb_dll(dll_path, sizeof(dll_path))) {
        return false;
    }

    char resolved_dir[512] = "";
    if (!resolve_db_dir(db_dir, resolved_dir, sizeof(resolved_dir))) {
        return false;
    }

    // Set DLL directory to DLL's parent folder
    char dll_dir[512] = "";
    snprintf(dll_dir, sizeof(dll_dir), "%s", dll_path);
    char *last_sep = strrchr(dll_dir, '\\');
    if (!last_sep) last_sep = strrchr(dll_dir, '/');
    if (last_sep) *last_sep = '\0';
    SetDllDirectoryA(dll_dir);

    s_egdb_module = LoadLibraryA(dll_path);
    if (!s_egdb_module) {
        return false;
    }

    s_fn_identify = (egdb_identify_fn)GetProcAddress(s_egdb_module, "egdb_identify");
    s_fn_open = (egdb_open_fn)GetProcAddress(s_egdb_module, "egdb_open");

    if (!s_fn_identify || !s_fn_open) {
        wld_egdb_close();
        return false;
    }

    int db_type = 0;
    int max_pieces = 0;
    if (s_fn_identify(resolved_dir, &db_type, &max_pieces) != 0) {
        wld_egdb_close();
        return false;
    }

    if (max_pieces < 2) {
        wld_egdb_close();
        return false;
    }

    int alloc_cache = (cache_mb > 0) ? cache_mb : 128;
    s_egdb_handle = s_fn_open(db_type, max_pieces, alloc_cache, resolved_dir, egdb_msg_callback);
    if (!s_egdb_handle || !s_egdb_handle->lookup) {
        wld_egdb_close();
        return false;
    }

    s_max_pieces = max_pieces;
    size_t missing = 0;
    s_loaded_slices = wld_egdb_scan_slices(resolved_dir, NULL, &missing);
    s_is_ready = true;
    return true;
#endif
}

void wld_egdb_close(void) {
#ifdef _WIN32
    if (s_egdb_handle && s_egdb_handle->close) {
        s_egdb_handle->close(s_egdb_handle);
    }
    s_egdb_handle = NULL;

    if (s_egdb_module) {
        FreeLibrary(s_egdb_module);
        s_egdb_module = NULL;
    }
    s_fn_identify = NULL;
    s_fn_open = NULL;
#endif
    s_is_ready = false;
    s_loaded_slices = 0;
    s_max_pieces = 0;
}

bool wld_egdb_is_ready(void) {
    return s_is_ready;
}

size_t wld_egdb_get_loaded_slices(void) {
    return s_loaded_slices;
}

int wld_egdb_get_max_pieces(void) {
    return s_max_pieces;
}

int wld_egdb_lookup_raw(EGDB_POSITION *pos, int color, int cl) {
#ifdef _WIN32
    if (!s_is_ready || !s_egdb_handle || !s_egdb_handle->lookup || !pos) {
        return 0; // EGDB_UNKNOWN
    }
    return s_egdb_handle->lookup(s_egdb_handle, pos, color, cl);
#else
    (void)pos; (void)color; (void)cl;
    return 0;
#endif
}

WLDValue wld_egdb_probe(const GameState *game) {
#ifdef _WIN32
    if (!s_is_ready || !s_egdb_handle || !game) {
        return WLD_UNKNOWN;
    }

    // Direct game terminal check
    if (game->is_game_over) {
        if (game->is_draw) return WLD_DRAW;
        if (game->winner == PLAYER_WHITE) return WLD_WIN_WHITE;
        if (game->winner == PLAYER_BLACK) return WLD_WIN_BLACK;
    }

    const Board *b = &game->board;
    uint32_t wm = b->white_men;
    uint32_t wk = b->white_kings;
    uint32_t bm = b->black_men;
    uint32_t bk = b->black_kings;

    int w_total = __builtin_popcount(wm) + __builtin_popcount(wk);
    int b_total = __builtin_popcount(bm) + __builtin_popcount(bk);

    if (w_total == 0) return WLD_WIN_BLACK;
    if (b_total == 0) return WLD_WIN_WHITE;
    if (w_total + b_total > s_max_pieces) return WLD_UNKNOWN;

    // Check if player has moves
    MoveList moves = *game_get_valid_moves(game);
    if (moves.count == 0) {
        return (game->current_player == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;
    }

    // In-flight capture resolution:
    // Endgame tablebases index quiescent states. If the position has legal forced jumps,
    // evaluate through minimax across legal capture branches.
    if (MOVE_IS_CAP(moves.moves[0])) {
        bool all_losses = true;
        for (int i = 0; i < moves.count; i++) {
            GameState next = *game;
            game_execute_move(&next, moves.moves[i]);
            WLDValue child_res = wld_egdb_probe(&next);
            if (game->current_player == PLAYER_WHITE) {
                if (child_res == WLD_WIN_WHITE) return WLD_WIN_WHITE;
                if (child_res != WLD_WIN_BLACK) all_losses = false;
            } else {
                if (child_res == WLD_WIN_BLACK) return WLD_WIN_BLACK;
                if (child_res != WLD_WIN_WHITE) all_losses = false;
            }
        }
        if (all_losses) {
            return (game->current_player == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;
        }
        return WLD_DRAW;
    }

    // Direct quiescent EGDB lookup
    EGDB_POSITION pos;
    pos.white = wm | wk;
    pos.black = bm | bk;
    pos.king  = wk | bk;

    // EGDB side to move: 0 = White, 1 = Black
    int color = (game->current_player == PLAYER_WHITE) ? 0 : 1;
    int res = s_egdb_handle->lookup(s_egdb_handle, &pos, color, 0);

    if (res == 1) { // WIN for current player
        return (game->current_player == PLAYER_WHITE) ? WLD_WIN_WHITE : WLD_WIN_BLACK;
    } else if (res == 2) { // LOSS for current player
        return (game->current_player == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;
    } else if (res == 0) { // DRAW
        return WLD_DRAW;
    }

    return WLD_UNKNOWN;
#else
    (void)game;
    return WLD_UNKNOWN;
#endif
}
