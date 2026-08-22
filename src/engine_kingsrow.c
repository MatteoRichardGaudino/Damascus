/*______________________________________________________________________________
  Kingsrow Italian Checkers Engine (Bridge & IPC Interface)
  Connects to kr_bridge.exe (via Wine on POSIX or natively on Windows)
______________________________________________________________________________*/

#include "engine_kingsrow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h>
#endif

#define CB_FREE 0
#define CB_WHITE 1
#define CB_BLACK 2
#define CB_MAN 4
#define CB_KING 8

typedef struct {
    bool is_connected;
    bool db_status_logged;
    double search_time;
#ifdef _WIN32
    HANDLE h_process;
    HANDLE h_stdin_write;
    HANDLE h_stdout_read;
#else
    pid_t pid;
    int pipe_in[2];  // Parent writes to pipe_in[1] -> Child reads from pipe_in[0]
    int pipe_out[2]; // Child writes to pipe_out[1] -> Parent reads from pipe_out[0]
    FILE *stream_in;
    FILE *stream_out;
#endif
} KingsrowEngineState;


#ifdef _WIN32
static bool read_line_with_timeout(HANDLE h_pipe, char *buf, size_t max_len, int timeout_ms) {
    if (!h_pipe || !buf || max_len == 0) return false;
    
    DWORD start = GetTickCount();
    size_t idx = 0;
    
    while (idx < max_len - 1) {
        DWORD avail = 0;
        if (!PeekNamedPipe(h_pipe, NULL, 0, NULL, &avail, NULL)) {
            break;
        }
        
        if (avail > 0) {
            char c = 0;
            DWORD read_bytes = 0;
            if (!ReadFile(h_pipe, &c, 1, &read_bytes, NULL) || read_bytes == 0) {
                break;
            }
            if (c == '\r') continue;
            buf[idx++] = c;
            if (c == '\n') break;
        } else {
            if (timeout_ms > 0 && (int)(GetTickCount() - start) > timeout_ms) {
                break;
            }
            Sleep(2);
        }
    }
    buf[idx] = '\0';
    return idx > 0;
}
#else
static bool read_line_with_timeout(int fd, char *buf, size_t max_len, int timeout_ms) {
    if (fd < 0 || !buf || max_len == 0) return false;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(fd + 1, &read_fds, NULL, NULL, &tv);
    if (ret <= 0) return false;
    
    size_t idx = 0;
    while (idx < max_len - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
        if (c == '\r') continue;
        buf[idx++] = c;
        if (c == '\n') break;
    }
    buf[idx] = '\0';
    return idx > 0;
}
#endif

static bool find_kingsrow_files(char *bridge_path, size_t b_len, char *dll_path, size_t d_len) {
    const char *candidates_bridge[] = {
        "third_party/engines/kingsrow_italian/app/engines/kr_bridge.exe",
        "../third_party/engines/kingsrow_italian/app/engines/kr_bridge.exe",
        "../../third_party/engines/kingsrow_italian/app/engines/kr_bridge.exe",
        "third_party/kr_bridge/kr_bridge.exe",
        "../third_party/kr_bridge/kr_bridge.exe",
        "./kr_bridge.exe"
    };
    const char *candidates_dll[] = {
        "third_party/engines/kingsrow_italian/app/engines/kr_italian64.dll",
        "../third_party/engines/kingsrow_italian/app/engines/kr_italian64.dll",
        "../../third_party/engines/kingsrow_italian/app/engines/kr_italian64.dll",
        "kr_italian64.dll"
    };

    bool bridge_found = false;
    for (size_t i = 0; i < sizeof(candidates_bridge)/sizeof(candidates_bridge[0]); i++) {
        FILE *f = fopen(candidates_bridge[i], "rb");
        if (f) {
            fclose(f);
#ifndef _WIN32
            char resolved[1024];
            if (realpath(candidates_bridge[i], resolved)) {
                snprintf(bridge_path, b_len, "%s", resolved);
            } else {
                snprintf(bridge_path, b_len, "%s", candidates_bridge[i]);
            }
#else
            char resolved[1024];
            if (GetFullPathNameA(candidates_bridge[i], sizeof(resolved), resolved, NULL)) {
                snprintf(bridge_path, b_len, "%s", resolved);
            } else {
                snprintf(bridge_path, b_len, "%s", candidates_bridge[i]);
            }
#endif
            bridge_found = true;
            break;
        }
    }

    bool dll_found = false;
    for (size_t i = 0; i < sizeof(candidates_dll)/sizeof(candidates_dll[0]); i++) {
        FILE *f = fopen(candidates_dll[i], "rb");
        if (f) {
            fclose(f);
#ifndef _WIN32
            char resolved[1024];
            if (realpath(candidates_dll[i], resolved)) {
                snprintf(dll_path, d_len, "%s", resolved);
            } else {
                snprintf(dll_path, d_len, "%s", candidates_dll[i]);
            }
#else
            char resolved[1024];
            if (GetFullPathNameA(candidates_dll[i], sizeof(resolved), resolved, NULL)) {
                snprintf(dll_path, d_len, "%s", resolved);
            } else {
                snprintf(dll_path, d_len, "%s", candidates_dll[i]);
            }
#endif
            dll_found = true;
            break;
        }
    }

    return bridge_found && dll_found;
}

static bool start_kingsrow_process(KingsrowEngineState *ks) {
    if (!ks) return false;

    char bridge_path[512] = "";
    char dll_path[512] = "";
    if (!find_kingsrow_files(bridge_path, sizeof(bridge_path), dll_path, sizeof(dll_path))) {
        return false;
    }

    // Determine engine directory
    char engine_dir[512] = "";
    snprintf(engine_dir, sizeof(engine_dir), "%s", bridge_path);
    char *last_slash = strrchr(engine_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        last_slash = strrchr(engine_dir, '\\');
        if (last_slash) *last_slash = '\0';
    }

#ifdef _WIN32
    // Windows Native Process Creation with Anonymous Pipes
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE h_stdin_read = NULL;
    HANDLE h_stdout_write = NULL;

    if (!CreatePipe(&h_stdin_read, &ks->h_stdin_write, &sa, 0)) return false;
    SetHandleInformation(ks->h_stdin_write, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&ks->h_stdout_read, &h_stdout_write, &sa, 0)) {
        CloseHandle(h_stdin_read);
        CloseHandle(ks->h_stdin_write);
        return false;
    }
    SetHandleInformation(ks->h_stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = h_stdin_read;
    si.hStdOutput = h_stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", bridge_path, dll_path);

    const char *work_dir = (engine_dir[0] != '\0') ? engine_dir : NULL;
    BOOL success = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, work_dir, &si, &pi);
    CloseHandle(h_stdin_read);
    CloseHandle(h_stdout_write);

    if (!success) {
        CloseHandle(ks->h_stdin_write);
        CloseHandle(ks->h_stdout_read);
        return false;
    }

    ks->h_process = pi.hProcess;
    CloseHandle(pi.hThread);

    // Handshake: Send PING and wait for READY + PONG
    DWORD written = 0;
    WriteFile(ks->h_stdin_write, "PING\n", 5, &written, NULL);

    char reply[256] = "";
    bool got_ready = false;
    bool got_pong = false;
    for (int attempts = 0; attempts < 20; attempts++) {
        if (read_line_with_timeout(ks->h_stdout_read, reply, sizeof(reply), 3000)) {
            if (strncmp(reply, "READY", 5) == 0) got_ready = true;
            if (strncmp(reply, "PONG", 4) == 0) got_pong = true;
            if (got_ready && got_pong) {
                ks->is_connected = true;
                break;
            }
        } else {
            break;
        }
    }

    if (!ks->is_connected) {
        return false;
    }

    // Configure Endgame Database (WLD)
    const char *cfg_cmds[] = {
        "COMMAND set dbpath ../db\n",
        "COMMAND set enable_wld 1\n",
        "COMMAND set max_dbpieces 8\n",
        "COMMAND set dbmbytes 128\n"
    };
    for (size_t c = 0; c < sizeof(cfg_cmds)/sizeof(cfg_cmds[0]); c++) {
        WriteFile(ks->h_stdin_write, cfg_cmds[c], (DWORD)strlen(cfg_cmds[c]), &written, NULL);
        char ack[256] = "";
        read_line_with_timeout(ks->h_stdout_read, ack, sizeof(ack), 1000);
    }

    return true;
#else
    // POSIX Subprocess Creation via Wine
    if (!engine_is_wine_available()) {
        return false;
    }

    if (pipe(ks->pipe_in) < 0) return false;
    if (pipe(ks->pipe_out) < 0) {
        close(ks->pipe_in[0]);
        close(ks->pipe_in[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(ks->pipe_in[0]); close(ks->pipe_in[1]);
        close(ks->pipe_out[0]); close(ks->pipe_out[1]);
        return false;
    }

    if (pid == 0) {
        // Child process
        dup2(ks->pipe_in[0], STDIN_FILENO);
        dup2(ks->pipe_out[1], STDOUT_FILENO);

        close(ks->pipe_in[0]); close(ks->pipe_in[1]);
        close(ks->pipe_out[0]); close(ks->pipe_out[1]);

        if (engine_dir[0] != '\0') {
            chdir(engine_dir);
        }

        // Find wine binary
        const char *wine_bin = "wine";
        if (access("/usr/local/bin/wine", X_OK) == 0) wine_bin = "/usr/local/bin/wine";
        else if (access("/opt/homebrew/bin/wine", X_OK) == 0) wine_bin = "/opt/homebrew/bin/wine";
        else if (access("/usr/local/bin/wine64", X_OK) == 0) wine_bin = "/usr/local/bin/wine64";
        else if (access("/opt/homebrew/bin/wine64", X_OK) == 0) wine_bin = "/opt/homebrew/bin/wine64";

        execlp(wine_bin, wine_bin, "kr_bridge.exe", "kr_italian64.dll", (char*)NULL);
        _exit(127);
    }

    // Parent process
    ks->pid = pid;
    close(ks->pipe_in[0]);
    close(ks->pipe_out[1]);

    ks->stream_in = fdopen(ks->pipe_in[1], "w");
    ks->stream_out = fdopen(ks->pipe_out[0], "r");

    if (!ks->stream_in || !ks->stream_out) {
        return false;
    }

    // Test handshake PING with timeout (10000ms for Wine initialization)
    fprintf(ks->stream_in, "PING\n");
    fflush(ks->stream_in);

    char reply[256] = "";
    int fd_read = fileno(ks->stream_out);
    bool got_ready = false;
    bool got_pong = false;
    for (int attempts = 0; attempts < 20; attempts++) {
        if (read_line_with_timeout(fd_read, reply, sizeof(reply), 5000)) {
            if (strncmp(reply, "READY", 5) == 0) got_ready = true;
            if (strncmp(reply, "PONG", 4) == 0) got_pong = true;
            if (got_ready && got_pong) {
                ks->is_connected = true;
                break;
            }
        } else {
            break;
        }
    }

    if (!ks->is_connected) {
        return false;
    }

    // Configure Endgame Database (WLD)
    const char *cfg_cmds[] = {
        "COMMAND set dbpath ../db\n",
        "COMMAND set enable_wld 1\n",
        "COMMAND set max_dbpieces 8\n",
        "COMMAND set dbmbytes 128\n"
    };
    for (size_t c = 0; c < sizeof(cfg_cmds)/sizeof(cfg_cmds[0]); c++) {
        fputs(cfg_cmds[c], ks->stream_in);
        fflush(ks->stream_in);
        char ack[256] = "";
        read_line_with_timeout(fd_read, ack, sizeof(ack), 1000);
    }

    return true;
#endif
}

bool engine_kingsrow_is_available(void) {
    char bridge_path[512] = "";
    char dll_path[512] = "";
    if (!find_kingsrow_files(bridge_path, sizeof(bridge_path), dll_path, sizeof(dll_path))) {
        return false;
    }

#ifdef _WIN32
    return true;
#else
    return engine_is_wine_available();
#endif
}

void engine_kingsrow_init(void **state) {
    KingsrowEngineState *ks = (KingsrowEngineState*)calloc(1, sizeof(KingsrowEngineState));
    if (ks) {
        ks->search_time = 1.0; // Default 1.0s search
        start_kingsrow_process(ks);
    }
    *state = ks;
}

Move engine_kingsrow_get_move(void *state, const GameState *game) {
    if (!state || !game || game->is_game_over) return MOVE_NONE;

    KingsrowEngineState *ks = (KingsrowEngineState*)state;
    if (!ks->is_connected) {
        // Retry connection if not yet connected
        if (!start_kingsrow_process(ks)) {
            return MOVE_NONE;
        }
    }

    int color = (game->current_player == PLAYER_WHITE) ? CB_WHITE : CB_BLACK;
    double maxtime = (ks->search_time > 0.0) ? ks->search_time : 1.0;

    // Convert Damascus bitboard to CheckerBoard format: b[col_x][row_y]
    int b[8][8];
    memset(b, 0, sizeof(b));
    for (int sq = 0; sq < 32; sq++) {
        PieceType p = board_get_piece_at(&game->board, sq);
        if (p == PIECE_NONE) continue;
        
        int cb_val = CB_FREE;
        if (p == PIECE_WHITE_PAWN) cb_val = CB_WHITE | CB_MAN;
        else if (p == PIECE_WHITE_DAMA) cb_val = CB_WHITE | CB_KING;
        else if (p == PIECE_BLACK_PAWN) cb_val = CB_BLACK | CB_MAN;
        else if (p == PIECE_BLACK_DAMA) cb_val = CB_BLACK | CB_KING;

        int r = SQ_TO_ROW(sq);
        int c = SQ_TO_COL(sq);
        int y_cb = 7 - r;
        int x_cb = 7 - c;
        b[x_cb][y_cb] = cb_val;
    }

    char cmd[2048];
    int pos = snprintf(cmd, sizeof(cmd), "GETMOVE %d %.2f", color, maxtime);
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %d", b[x][y]);
        }
    }
    snprintf(cmd + pos, sizeof(cmd) - pos, "\n");

#ifdef _WIN32
    DWORD written = 0;
    if (!WriteFile(ks->h_stdin_write, cmd, (DWORD)strlen(cmd), &written, NULL)) {
        return MOVE_NONE;
    }

    char response[1024] = "";
    bool got_move = false;
    int timeout_ms = (int)(maxtime * 1000) + 25000;
    while (read_line_with_timeout(ks->h_stdout_read, response, sizeof(response), timeout_ms)) {
        if (strncmp(response, "MOVE ", 5) == 0) {
            got_move = true;
            break;
        }
    }
    if (!got_move) {
        const MoveList *fallback = game_get_valid_moves(game);
        if (fallback->count > 0) return fallback->moves[0];
        return MOVE_NONE;
    }

    if (!ks->db_status_logged) {
        ks->db_status_logged = true;
        WriteFile(ks->h_stdin_write, "COMMAND about\n", 14, &written, NULL);
        char about_buf[1024] = "";
        while (read_line_with_timeout(ks->h_stdout_read, about_buf, sizeof(about_buf), 1000)) {
            if (strstr(about_buf, "endgame database") || strstr(about_buf, "WLD")) {
                char *clean = (strncmp(about_buf, "REPLY ", 6) == 0) ? about_buf + 6 : about_buf;
                fprintf(stderr, "\n[Kingsrow] %s\n", clean);
                break;
            }
        }
    }
#else
    if (!ks->stream_in || !ks->stream_out) return MOVE_NONE;
    fputs(cmd, ks->stream_in);
    fflush(ks->stream_in);

    char response[1024] = "";
    int fd_read = fileno(ks->stream_out);
    bool got_move = false;
    // Allow up to 25 seconds for first search (tablebase loading) and generous headroom for subsequent searches
    while (read_line_with_timeout(fd_read, response, sizeof(response), (int)(maxtime * 1000) + 25000)) {
        if (strncmp(response, "MOVE ", 5) == 0) {
            got_move = true;
            break;
        }
    }
    if (!got_move) {
        const MoveList *fallback = game_get_valid_moves(game);
        if (fallback->count > 0) return fallback->moves[0];
        return MOVE_NONE;
    }

    if (!ks->db_status_logged) {
        ks->db_status_logged = true;
        fputs("COMMAND about\n", ks->stream_in);
        fflush(ks->stream_in);
        char about_buf[1024] = "";
        while (read_line_with_timeout(fd_read, about_buf, sizeof(about_buf), 1000)) {
            if (strstr(about_buf, "endgame database") || strstr(about_buf, "WLD")) {
                char *clean = (strncmp(about_buf, "REPLY ", 6) == 0) ? about_buf + 6 : about_buf;
                fprintf(stderr, "\n[Kingsrow] %s\n", clean);
                break;
            }
        }
    }
#endif

    // Parse response format: "MOVE <res> <jumps> <from_x> <from_y> <to_x> <to_y> ..."
    int res = 0, jumps = 0, fx = -1, fy = -1, tx = -1, ty = -1;
    if (sscanf(response, "MOVE %d %d %d %d %d %d", &res, &jumps, &fx, &fy, &tx, &ty) >= 6) {
        int from_row = 7 - fy;
        int from_col = 7 - fx;
        int to_row = 7 - ty;
        int to_col = 7 - tx;
        int from_sq = ROW_COL_TO_SQ(from_row, from_col);
        int to_sq = ROW_COL_TO_SQ(to_row, to_col);

        const MoveList *valid_moves = game_get_valid_moves(game);
        for (int i = 0; i < valid_moves->count; i++) {
            Move m = valid_moves->moves[i];
            if (MOVE_FROM(m) == from_sq && MOVE_TO(m) == to_sq) {
                return m;
            }
        }

        for (int i = 0; i < valid_moves->count; i++) {
            Move m = valid_moves->moves[i];
            if (MOVE_FROM(m) == from_sq) {
                return m;
            }
        }

        if (valid_moves->count > 0) return valid_moves->moves[0];
    }

    const MoveList *valid_moves = game_get_valid_moves(game);
    if (valid_moves->count > 0) return valid_moves->moves[0];
    return MOVE_NONE;
}

void engine_kingsrow_cleanup(void *state) {
    if (!state) return;
    KingsrowEngineState *ks = (KingsrowEngineState*)state;

#ifdef _WIN32
    if (ks->is_connected && ks->h_stdin_write) {
        DWORD written = 0;
        WriteFile(ks->h_stdin_write, "QUIT\n", 5, &written, NULL);
        CloseHandle(ks->h_stdin_write);
        CloseHandle(ks->h_stdout_read);
        WaitForSingleObject(ks->h_process, 1000);
        CloseHandle(ks->h_process);
    }
#else
    if (ks->is_connected && ks->stream_in) {
        fprintf(ks->stream_in, "QUIT\n");
        fflush(ks->stream_in);
        fclose(ks->stream_in);
        fclose(ks->stream_out);
        close(ks->pipe_in[1]);
        close(ks->pipe_out[0]);
        kill(ks->pid, SIGTERM);
        waitpid(ks->pid, NULL, WNOHANG);
    }
#endif
    free(ks);
}

void engine_kingsrow_set_search_time(void *state, double seconds) {
    if (state && seconds > 0.0) {
        ((KingsrowEngineState*)state)->search_time = seconds;
    }
}

