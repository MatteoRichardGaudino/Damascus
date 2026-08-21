#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CB_FREE 0
#define CB_WHITE 1
#define CB_BLACK 2
#define CB_MAN 4
#define CB_KING 8

#define CB_DRAW 0
#define CB_WIN 1
#define CB_LOSS 2
#define CB_UNKNOWN 3

struct coor {
    int x;
    int y;
};

struct CBmove {
    int jumps;
    int newpiece;
    int oldpiece;
    struct coor from;
    struct coor to;
    struct coor path[12];
    struct coor del[12];
    int delpiece[12];
};

typedef int (WINAPI *CB_GETMOVE)(int board[8][8], int color, double maxtime, char str[1024], int *playnow, int info, int unused, struct CBmove *move);
typedef int (WINAPI *CB_GETSTRING)(char str[255]);
typedef int (WINAPI *CB_ENGINECOMMAND)(const char *command, char reply[1024]);

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    const char *dll_path = "kr_italian64.dll";
    if (argc > 1) {
        dll_path = argv[1];
    }

    HMODULE hDll = LoadLibraryA(dll_path);
    if (!hDll) {
        fprintf(stderr, "ERROR: Could not load DLL %s (Error code: %lu)\n", dll_path, GetLastError());
        return 1;
    }

    CB_GETMOVE pGetMove = (CB_GETMOVE)GetProcAddress(hDll, "getmove");
    CB_ENGINECOMMAND pEngineCommand = (CB_ENGINECOMMAND)GetProcAddress(hDll, "enginecommand");
    CB_GETSTRING pGetAbout = (CB_GETSTRING)GetProcAddress(hDll, "getengineabout");

    if (!pGetMove) {
        fprintf(stderr, "ERROR: getmove function not found in DLL %s\n", dll_path);
        FreeLibrary(hDll);
        return 2;
    }

    printf("READY %s\n", dll_path);
    fflush(stdout);

    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        if (strcmp(line, "PING") == 0) {
            printf("PONG\n");
            fflush(stdout);
        } else if (strcmp(line, "QUIT") == 0) {
            printf("BYE\n");
            fflush(stdout);
            break;
        } else if (strcmp(line, "ABOUT") == 0) {
            char about[1024] = "Kingsrow Italian Engine";
            if (pGetAbout) {
                pGetAbout(about);
            }
            printf("ABOUT %s\n", about);
            fflush(stdout);
        } else if (strncmp(line, "COMMAND ", 8) == 0) {
            char reply[1024] = "";
            if (pEngineCommand) {
                pEngineCommand(line + 8, reply);
            }
            printf("REPLY %s\n", reply);
            fflush(stdout);
        } else if (strncmp(line, "GETMOVE ", 8) == 0) {
            int color = 1;
            double maxtime = 1.0;
            int board[8][8];
            memset(board, 0, sizeof(board));

            char *ptr = line + 8;
            color = (int)strtol(ptr, &ptr, 10);
            maxtime = strtod(ptr, &ptr);

            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    board[x][y] = (int)strtol(ptr, &ptr, 10);
                }
            }

            char eval_str[1024] = "";
            int playnow = 0;
            struct CBmove move;
            memset(&move, 0, sizeof(move));

            int res = pGetMove(board, color, maxtime, eval_str, &playnow, 0, 0, &move);

            int to_x = move.to.x;
            int to_y = move.to.y;

            printf("MOVE %d %d %d %d %d %d %d %d %s\n",
                   res,
                   move.jumps,
                   move.from.x, move.from.y,
                   to_x, to_y,
                   move.newpiece, move.oldpiece,
                   eval_str[0] ? eval_str : "OK");
            fflush(stdout);
        } else {
            printf("UNKNOWN_COMMAND\n");
            fflush(stdout);
        }
    }

    FreeLibrary(hDll);
    return 0;
}
