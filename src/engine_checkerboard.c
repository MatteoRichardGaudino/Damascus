/*______________________________________________________________________________
  CheckerBoard Italian Checkers Engine (Dama Italiana)
  Original Engine Author: Martin Fierz (CheckerBoard Dama Engine)
  Adapted for Damascus 3D
______________________________________________________________________________*/

#include "engine_checkerboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CB_FREE 0
#define CB_WHITE 1
#define CB_BLACK 2
#define CB_MAN 4
#define CB_KING 8

#define OCCUPIED 0
#define FREE 16
#define MAXDEPTH 64
#define MAXMOVES 32

#define CB_CHANGECOLOR(color) ((color) ^ (CB_WHITE | CB_BLACK))

typedef struct {
    int x;
    int y;
} CBCoor;

typedef struct {
    int jumps;
    int newpiece;
    int oldpiece;
    CBCoor from;
    CBCoor to;
    CBCoor path[12];
    CBCoor del[12];
    int delpiece[12];
} CBCBMove;

typedef struct {
    short n;
    int m[12];
} CBMove2;

typedef struct {
    double search_time; // Thinking time in seconds
} CheckerboardEngineState;

// Internal engine globals for search state
static int g_alphabetas = 0;
static int g_playnow = 0;
static CBCBMove g_best_cb_move;
static clock_t g_starttime;
static double g_absolute_maxtime = 1.0;

static CBCoor numbertocoor(int n) {
    CBCoor c;
    switch (n) {
        case 5:  c.x = 0; c.y = 0; break;
        case 6:  c.x = 2; c.y = 0; break;
        case 7:  c.x = 4; c.y = 0; break;
        case 8:  c.x = 6; c.y = 0; break;
        case 10: c.x = 1; c.y = 1; break;
        case 11: c.x = 3; c.y = 1; break;
        case 12: c.x = 5; c.y = 1; break;
        case 13: c.x = 7; c.y = 1; break;
        case 14: c.x = 0; c.y = 2; break;
        case 15: c.x = 2; c.y = 2; break;
        case 16: c.x = 4; c.y = 2; break;
        case 17: c.x = 6; c.y = 2; break;
        case 19: c.x = 1; c.y = 3; break;
        case 20: c.x = 3; c.y = 3; break;
        case 21: c.x = 5; c.y = 3; break;
        case 22: c.x = 7; c.y = 3; break;
        case 23: c.x = 0; c.y = 4; break;
        case 24: c.x = 2; c.y = 4; break;
        case 25: c.x = 4; c.y = 4; break;
        case 26: c.x = 6; c.y = 4; break;
        case 28: c.x = 1; c.y = 5; break;
        case 29: c.x = 3; c.y = 5; break;
        case 30: c.x = 5; c.y = 5; break;
        case 31: c.x = 7; c.y = 5; break;
        case 32: c.x = 0; c.y = 6; break;
        case 33: c.x = 2; c.y = 6; break;
        case 34: c.x = 4; c.y = 6; break;
        case 35: c.x = 6; c.y = 6; break;
        case 37: c.x = 1; c.y = 7; break;
        case 38: c.x = 3; c.y = 7; break;
        case 39: c.x = 5; c.y = 7; break;
        case 40: c.x = 7; c.y = 7; break;
        default: c.x = 0; c.y = 0; break;
    }
    return c;
}

static void setbestmove(CBMove2 move) {
    int jumps = move.n - 2;
    int from = move.m[0] % 256;
    int to = move.m[1] % 256;

    g_best_cb_move.from = numbertocoor(from);
    g_best_cb_move.to = numbertocoor(to);
    g_best_cb_move.jumps = jumps;
    g_best_cb_move.newpiece = ((move.m[1] >> 16) % 256);
    g_best_cb_move.oldpiece = ((move.m[0] >> 8) % 256);

    for (int i = 2; i < move.n; i++) {
        g_best_cb_move.delpiece[i - 2] = ((move.m[i] >> 8) % 256);
        g_best_cb_move.del[i - 2] = numbertocoor(move.m[i] % 256);
    }

    if (jumps > 1) {
        CBCoor c1 = numbertocoor(from);
        for (int i = 2; i < move.n; i++) {
            CBCoor c2 = numbertocoor(move.m[i] % 256);
            if (c2.x > c1.x) c2.x++;
            else c2.x--;
            if (c2.y > c1.y) c2.y++;
            else c2.y--;
            g_best_cb_move.path[i - 1] = c2;
            c1 = c2;
        }
    } else {
        g_best_cb_move.path[1] = numbertocoor(to);
    }
}

static void domove(int b[46], const CBMove2 *move) {
    for (int i = 0; i < move->n; i++) {
        int square = (move->m[i] % 256);
        int after = ((move->m[i] >> 16) % 256);
        b[square] = after;
    }
}

static void undomove(int b[46], const CBMove2 *move) {
    for (int i = move->n - 1; i >= 0; --i) {
        int square = (move->m[i] % 256);
        int before = ((move->m[i] >> 8) % 256);
        b[square] = before;
    }
}

// Prototypes for move generation & search
static int generatemovelist(int b[46], CBMove2 movelist[MAXMOVES], int color);
static int generatecapturelist(int b[46], CBMove2 movelist[MAXMOVES], int color);
static void blackmancapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square);
static void blackkingcapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square);
static void whitemancapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square);
static void whitekingcapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square);
static int testcapture(int b[46], int color);
static int evaluation(int b[46], int color);
static int alphabeta(int b[46], int depth, int alpha, int beta, int color);
static int firstalphabeta(int b[46], int depth, int alpha, int beta, int color, CBMove2 *best);

// Implementation of Evaluation Function (Martin Fierz positional heuristics)
static int evaluation(int b[46], int color) {
    static const int edge[14] = { 5, 6, 7, 8, 13, 14, 22, 23, 31, 32, 37, 38, 39, 40 };
    static const int center[8] = { 15, 16, 20, 21, 24, 25, 29, 30 };
    static const int safeedge[4] = { 8, 13, 32, 37 };
    static const int row[41] = {
        0,0,0,0,0,0,0,0,0,0,
        1,1,1,1,2,2,2,2,0,
        3,3,3,3,4,4,4,4,0,
        5,5,5,5,6,6,6,6,0,
        7,7,7,7
    };

    int nbm = 0, nbk = 0, nwm = 0, nwk = 0;
    int nbmc = 0, nbkc = 0, nwmc = 0, nwkc = 0;
    int nbme = 0, nbke = 0, nwme = 0, nwke = 0;
    int code = 0;

    for (int i = 5; i <= 40; i++) {
        int piece = b[i];
        if (piece == FREE || piece == OCCUPIED) continue;

        if (piece == (CB_BLACK | CB_MAN)) {
            nbm++;
            code += 1;
        } else if (piece == (CB_BLACK | CB_KING)) {
            nbk++;
            code += 16;
        } else if (piece == (CB_WHITE | CB_MAN)) {
            nwm++;
            code += 256;
        } else if (piece == (CB_WHITE | CB_KING)) {
            nwk++;
            code += 4096;
        }
    }

    for (int i = 0; i < 8; i++) {
        int p = b[center[i]];
        if (p == (CB_BLACK | CB_MAN)) nbmc++;
        else if (p == (CB_BLACK | CB_KING)) nbkc++;
        else if (p == (CB_WHITE | CB_MAN)) nwmc++;
        else if (p == (CB_WHITE | CB_KING)) nwkc++;
    }

    for (int i = 0; i < 14; i++) {
        int p = b[edge[i]];
        if (p == (CB_BLACK | CB_MAN)) nbme++;
        else if (p == (CB_BLACK | CB_KING)) nbke++;
        else if (p == (CB_WHITE | CB_MAN)) nwme++;
        else if (p == (CB_WHITE | CB_KING)) nwke++;
    }

    int v1 = 100 * nbm + 145 * nbk;
    int v2 = 100 * nwm + 145 * nwk;
    int eval = v1 - v2;

    int nm = nbm + nwm;
    int nk = nbk + nwk;

    // Back rank protection
    int backrank = 0;
    if (b[5] == (CB_BLACK | CB_MAN)) backrank++;
    if (b[6] == (CB_BLACK | CB_MAN)) backrank++;
    if (b[7] == (CB_BLACK | CB_MAN)) backrank++;
    if (b[8] == (CB_BLACK | CB_MAN)) backrank++;
    if (b[37] == (CB_WHITE | CB_MAN)) backrank--;
    if (b[38] == (CB_WHITE | CB_MAN)) backrank--;
    if (b[39] == (CB_WHITE | CB_MAN)) backrank--;
    if (b[40] == (CB_WHITE | CB_MAN)) backrank--;
    eval += 3 * backrank;

    // Center & edge control
    eval += 5 * (nbkc - nwkc);
    eval += 1 * (nbmc - nwmc);
    eval -= 5 * (nbke - nwke);
    eval -= 1 * (nbme - nwme);

    // Safe edge protection
    for (int i = 0; i < 4; i++) {
        int p = b[safeedge[i]];
        if (p == (CB_BLACK | CB_MAN)) eval += 2;
        else if (p == (CB_WHITE | CB_MAN)) eval -= 2;
    }

    // Advancing pawns bonus
    int tempo = 0;
    for (int i = 5; i <= 40; i++) {
        if (b[i] == (CB_BLACK | CB_MAN)) tempo += row[i];
        else if (b[i] == (CB_WHITE | CB_MAN)) tempo -= (7 - row[i]);
    }
    if (nm > 12) eval -= 2 * tempo;
    else if (nm > 6) eval -= 1 * tempo;
    else eval += 2 * tempo;

    if (color == CB_BLACK) eval += 2;
    else eval -= 2;

    return eval;
}

// Alpha-Beta Search Implementation
static int firstalphabeta(int b[46], int depth, int alpha, int beta, int color, CBMove2 *best) {
    g_alphabetas++;
    if (g_playnow) return 0;

    int capture = testcapture(b, color);

    if (depth == 0) {
        if (capture == 0) return evaluation(b, color);
        else depth = 1;
    }

    CBMove2 movelist[MAXMOVES];
    int numberofmoves = 0;

    if (capture == 0) {
        numberofmoves = generatemovelist(b, movelist, color);
        if (numberofmoves == 0) {
            return (color == CB_BLACK) ? -5000 : 5000;
        }
    } else {
        numberofmoves = generatecapturelist(b, movelist, color);
    }

    if (numberofmoves > 0) {
        *best = movelist[0];
    }

    for (int i = 0; i < numberofmoves; i++) {
        domove(b, &movelist[i]);
        int val = alphabeta(b, depth - 1, alpha, beta, CB_CHANGECOLOR(color));
        undomove(b, &movelist[i]);

        if (color == CB_BLACK) {
            if (val >= beta) return val;
            if (val > alpha) {
                alpha = val;
                *best = movelist[i];
            }
        } else {
            if (val <= alpha) return val;
            if (val < beta) {
                beta = val;
                *best = movelist[i];
            }
        }
    }

    return (color == CB_BLACK) ? alpha : beta;
}

static int alphabeta(int b[46], int depth, int alpha, int beta, int color) {
    g_alphabetas++;
    if ((g_alphabetas & 0x3ff) == 0) {
        if ((double)(clock() - g_starttime) / CLOCKS_PER_SEC >= g_absolute_maxtime) {
            g_playnow = 1;
        }
    }
    if (g_playnow) return 0;

    int capture = testcapture(b, color);

    if (depth == 0) {
        if (capture == 0) return evaluation(b, color);
        else depth = 1;
    }

    CBMove2 movelist[MAXMOVES];
    int numberofmoves = 0;

    if (capture == 0) {
        numberofmoves = generatemovelist(b, movelist, color);
        if (numberofmoves == 0) {
            return (color == CB_BLACK) ? -5000 : 5000;
        }
    } else {
        numberofmoves = generatecapturelist(b, movelist, color);
    }

    for (int i = 0; i < numberofmoves; i++) {
        domove(b, &movelist[i]);
        int val = alphabeta(b, depth - 1, alpha, beta, CB_CHANGECOLOR(color));
        undomove(b, &movelist[i]);

        if (color == CB_BLACK) {
            if (val >= beta) return val;
            if (val > alpha) alpha = val;
        } else {
            if (val <= alpha) return val;
            if (val < beta) beta = val;
        }
    }

    return (color == CB_BLACK) ? alpha : beta;
}

static int checkers_search(int b[46], int color, double maxtime) {
    g_alphabetas = 0;
    g_playnow = 0;
    g_starttime = clock();
    g_absolute_maxtime = maxtime;

    CBMove2 movelist[MAXMOVES];
    int numberofmoves = generatecapturelist(b, movelist, color);
    if (numberofmoves == 0) {
        numberofmoves = generatemovelist(b, movelist, color);
    }

    if (numberofmoves == 0) return 0;

    setbestmove(movelist[0]);
    if (numberofmoves == 1) {
        domove(b, &movelist[0]);
        return 1;
    }

    CBMove2 best, lastbest;
    best = movelist[0];
    lastbest = movelist[0];

    int eval = firstalphabeta(b, 1, -10000, 10000, color, &best);
    for (int d = 2; d <= MAXDEPTH; d++) {
        if ((double)(clock() - g_starttime) / CLOCKS_PER_SEC >= maxtime) break;
        lastbest = best;
        eval = firstalphabeta(b, d, -10000, 10000, color, &best);

        if (g_playnow) break;
        if (eval >= 4000 || eval <= -4000) break;
    }

    if (g_playnow) best = lastbest;

    domove(b, &best);
    setbestmove(best);
    return eval;
}

// Move generation for Italian Checkers
static int generatemovelist(int b[46], CBMove2 movelist[MAXMOVES], int color) {
    int n = 0;
    int m;

    if (color == CB_BLACK) {
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_BLACK) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if ((b[i + 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        if (i >= 28) m = (CB_BLACK | CB_KING);
                        else m = (CB_BLACK | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i + 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i + 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        if (i >= 28) m = (CB_BLACK | CB_KING);
                        else m = (CB_BLACK | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i + 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                } else { // King
                    if ((b[i + 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i + 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i - 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i - 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                }
            }
        }
    } else { // CB_WHITE
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_WHITE) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if ((b[i - 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        if (i <= 17) m = (CB_WHITE | CB_KING);
                        else m = (CB_WHITE | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i - 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i - 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        if (i <= 17) m = (CB_WHITE | CB_KING);
                        else m = (CB_WHITE | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i - 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                } else { // King
                    if ((b[i + 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i + 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i - 4] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 4);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                    if ((b[i - 5] & FREE) != 0) {
                        movelist[n].n = 2;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 5);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        n++;
                    }
                }
            }
        }
    }
    return n;
}

// Italian Checkers Capture Generators & Law of Maximum Priority Filtering
static void blackmancapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square) {
    int found = 0;
    int i = square;
    CBMove2 orgmove = movelist[*n];
    CBMove2 move = orgmove;

    if (b[i + 4] == (CB_WHITE | CB_MAN)) {
        if ((b[i + 8] & FREE) != 0) {
            move.n++;
            int m = (i >= 28) ? (CB_BLACK | CB_KING) : (CB_BLACK | CB_MAN);
            m = (m << 8) + FREE;
            m = (m << 8) + (i + 8);
            move.m[1] = m;
            m = (FREE << 8) + b[i + 4];
            m = (m << 8) + (i + 4);
            move.m[move.n - 1] = m;
            found = 1;
            movelist[*n] = move;
            int tmp = b[i + 4];
            b[i + 4] = FREE;
            blackmancapture(b, n, movelist, i + 8);
            b[i + 4] = tmp;
        }
    }

    move = orgmove;
    if (b[i + 5] == (CB_WHITE | CB_MAN)) {
        if ((b[i + 10] & FREE) != 0) {
            move.n++;
            int m = (i >= 28) ? (CB_BLACK | CB_KING) : (CB_BLACK | CB_MAN);
            m = (m << 8) + FREE;
            m = (m << 8) + (i + 10);
            move.m[1] = m;
            m = (FREE << 8) + b[i + 5];
            m = (m << 8) + (i + 5);
            move.m[move.n - 1] = m;
            found = 1;
            movelist[*n] = move;
            int tmp = b[i + 5];
            b[i + 5] = FREE;
            blackmancapture(b, n, movelist, i + 10);
            b[i + 5] = tmp;
        }
    }

    if (!found) (*n)++;
}

static void blackkingcapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square) {
    int found = 0;
    int i = square;
    CBMove2 orgmove = movelist[*n];
    CBMove2 move = orgmove;

    if ((b[i + 4] & CB_WHITE) != 0 && (b[i + 8] & FREE) != 0) {
        move.n++;
        int m = (CB_BLACK | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i + 8);
        move.m[1] = m;
        m = (FREE << 8) + b[i + 4];
        m = (m << 8) + (i + 4);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i + 4];
        b[i + 4] = FREE;
        blackkingcapture(b, n, movelist, i + 8);
        b[i + 4] = tmp;
    }

    move = orgmove;
    if ((b[i + 5] & CB_WHITE) != 0 && (b[i + 10] & FREE) != 0) {
        move.n++;
        int m = (CB_BLACK | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i + 10);
        move.m[1] = m;
        m = (FREE << 8) + b[i + 5];
        m = (m << 8) + (i + 5);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i + 5];
        b[i + 5] = FREE;
        blackkingcapture(b, n, movelist, i + 10);
        b[i + 5] = tmp;
    }

    move = orgmove;
    if ((b[i - 4] & CB_WHITE) != 0 && (b[i - 8] & FREE) != 0) {
        move.n++;
        int m = (CB_BLACK | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i - 8);
        move.m[1] = m;
        m = (FREE << 8) + b[i - 4];
        m = (m << 8) + (i - 4);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i - 4];
        b[i - 4] = FREE;
        blackkingcapture(b, n, movelist, i - 8);
        b[i - 4] = tmp;
    }

    move = orgmove;
    if ((b[i - 5] & CB_WHITE) != 0 && (b[i - 10] & FREE) != 0) {
        move.n++;
        int m = (CB_BLACK | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i - 10);
        move.m[1] = m;
        m = (FREE << 8) + b[i - 5];
        m = (m << 8) + (i - 5);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i - 5];
        b[i - 5] = FREE;
        blackkingcapture(b, n, movelist, i - 10);
        b[i - 5] = tmp;
    }

    if (!found) (*n)++;
}

static void whitemancapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square) {
    int found = 0;
    int i = square;
    CBMove2 orgmove = movelist[*n];
    CBMove2 move = orgmove;

    if (b[i - 4] == (CB_BLACK | CB_MAN)) {
        if ((b[i - 8] & FREE) != 0) {
            move.n++;
            int m = (i <= 17) ? (CB_WHITE | CB_KING) : (CB_WHITE | CB_MAN);
            m = (m << 8) + FREE;
            m = (m << 8) + (i - 8);
            move.m[1] = m;
            m = (FREE << 8) + b[i - 4];
            m = (m << 8) + (i - 4);
            move.m[move.n - 1] = m;
            found = 1;
            movelist[*n] = move;
            int tmp = b[i - 4];
            b[i - 4] = FREE;
            whitemancapture(b, n, movelist, i - 8);
            b[i - 4] = tmp;
        }
    }

    move = orgmove;
    if (b[i - 5] == (CB_BLACK | CB_MAN)) {
        if ((b[i - 10] & FREE) != 0) {
            move.n++;
            int m = (i <= 17) ? (CB_WHITE | CB_KING) : (CB_WHITE | CB_MAN);
            m = (m << 8) + FREE;
            m = (m << 8) + (i - 10);
            move.m[1] = m;
            m = (FREE << 8) + b[i - 5];
            m = (m << 8) + (i - 5);
            move.m[move.n - 1] = m;
            found = 1;
            movelist[*n] = move;
            int tmp = b[i - 5];
            b[i - 5] = FREE;
            whitemancapture(b, n, movelist, i - 10);
            b[i - 5] = tmp;
        }
    }

    if (!found) (*n)++;
}

static void whitekingcapture(int b[46], int *n, CBMove2 movelist[MAXMOVES], int square) {
    int found = 0;
    int i = square;
    CBMove2 orgmove = movelist[*n];
    CBMove2 move = orgmove;

    if ((b[i + 4] & CB_BLACK) != 0 && (b[i + 8] & FREE) != 0) {
        move.n++;
        int m = (CB_WHITE | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i + 8);
        move.m[1] = m;
        m = (FREE << 8) + b[i + 4];
        m = (m << 8) + (i + 4);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i + 4];
        b[i + 4] = FREE;
        whitekingcapture(b, n, movelist, i + 8);
        b[i + 4] = tmp;
    }

    move = orgmove;
    if ((b[i + 5] & CB_BLACK) != 0 && (b[i + 10] & FREE) != 0) {
        move.n++;
        int m = (CB_WHITE | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i + 10);
        move.m[1] = m;
        m = (FREE << 8) + b[i + 5];
        m = (m << 8) + (i + 5);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i + 5];
        b[i + 5] = FREE;
        whitekingcapture(b, n, movelist, i + 10);
        b[i + 5] = tmp;
    }

    move = orgmove;
    if ((b[i - 4] & CB_BLACK) != 0 && (b[i - 8] & FREE) != 0) {
        move.n++;
        int m = (CB_WHITE | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i - 8);
        move.m[1] = m;
        m = (FREE << 8) + b[i - 4];
        m = (m << 8) + (i - 4);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i - 4];
        b[i - 4] = FREE;
        whitekingcapture(b, n, movelist, i - 8);
        b[i - 4] = tmp;
    }

    move = orgmove;
    if ((b[i - 5] & CB_BLACK) != 0 && (b[i - 10] & FREE) != 0) {
        move.n++;
        int m = (CB_WHITE | CB_KING);
        m = (m << 8) + FREE;
        m = (m << 8) + (i - 10);
        move.m[1] = m;
        m = (FREE << 8) + b[i - 5];
        m = (m << 8) + (i - 5);
        move.m[move.n - 1] = m;
        found = 1;
        movelist[*n] = move;
        int tmp = b[i - 5];
        b[i - 5] = FREE;
        whitekingcapture(b, n, movelist, i - 10);
        b[i - 5] = tmp;
    }

    if (!found) (*n)++;
}

static int generatecapturelist(int b[46], CBMove2 movelist[MAXMOVES], int color) {
    int n = 0;
    int m;
    int ismove[MAXMOVES];

    if (color == CB_BLACK) {
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_BLACK) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if (b[i + 4] == (CB_WHITE | CB_MAN) && (b[i + 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = (i >= 28) ? (CB_BLACK | CB_KING) : (CB_BLACK | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i + 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 4];
                        m = (m << 8) + (i + 4);
                        movelist[n].m[2] = m;
                        blackmancapture(b, &n, movelist, i + 8);
                    }
                    if (b[i + 5] == (CB_WHITE | CB_MAN) && (b[i + 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = (i >= 28) ? (CB_BLACK | CB_KING) : (CB_BLACK | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i + 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 5];
                        m = (m << 8) + (i + 5);
                        movelist[n].m[2] = m;
                        blackmancapture(b, &n, movelist, i + 10);
                    }
                } else { // King
                    if ((b[i + 4] & CB_WHITE) != 0 && (b[i + 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 4];
                        m = (m << 8) + (i + 4);
                        movelist[n].m[2] = m;
                        int tmp = b[i + 4];
                        b[i + 4] = FREE;
                        b[i] = FREE;
                        blackkingcapture(b, &n, movelist, i + 8);
                        b[i + 4] = tmp;
                        b[i] = CB_BLACK | CB_KING;
                    }
                    if ((b[i + 5] & CB_WHITE) != 0 && (b[i + 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 5];
                        m = (m << 8) + (i + 5);
                        movelist[n].m[2] = m;
                        int tmp = b[i + 5];
                        b[i + 5] = FREE;
                        b[i] = FREE;
                        blackkingcapture(b, &n, movelist, i + 10);
                        b[i + 5] = tmp;
                        b[i] = CB_BLACK | CB_KING;
                    }
                    if ((b[i - 4] & CB_WHITE) != 0 && (b[i - 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 4];
                        m = (m << 8) + (i - 4);
                        movelist[n].m[2] = m;
                        int tmp = b[i - 4];
                        b[i - 4] = FREE;
                        b[i] = FREE;
                        blackkingcapture(b, &n, movelist, i - 8);
                        b[i - 4] = tmp;
                        b[i] = CB_BLACK | CB_KING;
                    }
                    if ((b[i - 5] & CB_WHITE) != 0 && (b[i - 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_BLACK | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_BLACK | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 5];
                        m = (m << 8) + (i - 5);
                        movelist[n].m[2] = m;
                        int tmp = b[i - 5];
                        b[i - 5] = FREE;
                        b[i] = FREE;
                        blackkingcapture(b, &n, movelist, i - 10);
                        b[i - 5] = tmp;
                        b[i] = CB_BLACK | CB_KING;
                    }
                }
            }
        }
    } else { // CB_WHITE
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_WHITE) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if (b[i - 4] == (CB_BLACK | CB_MAN) && (b[i - 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = (i <= 17) ? (CB_WHITE | CB_KING) : (CB_WHITE | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i - 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 4];
                        m = (m << 8) + (i - 4);
                        movelist[n].m[2] = m;
                        whitemancapture(b, &n, movelist, i - 8);
                    }
                    if (b[i - 5] == (CB_BLACK | CB_MAN) && (b[i - 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = (i <= 17) ? (CB_WHITE | CB_KING) : (CB_WHITE | CB_MAN);
                        m = (m << 8) + FREE;
                        m = (m << 8) + (i - 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_MAN);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 5];
                        m = (m << 8) + (i - 5);
                        movelist[n].m[2] = m;
                        whitemancapture(b, &n, movelist, i - 10);
                    }
                } else { // King
                    if ((b[i + 4] & CB_BLACK) != 0 && (b[i + 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 4];
                        m = (m << 8) + (i + 4);
                        movelist[n].m[2] = m;
                        int tmp = b[i + 4];
                        b[i + 4] = FREE;
                        b[i] = FREE;
                        whitekingcapture(b, &n, movelist, i + 8);
                        b[i + 4] = tmp;
                        b[i] = CB_WHITE | CB_KING;
                    }
                    if ((b[i + 5] & CB_BLACK) != 0 && (b[i + 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i + 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i + 5];
                        m = (m << 8) + (i + 5);
                        movelist[n].m[2] = m;
                        int tmp = b[i + 5];
                        b[i + 5] = FREE;
                        b[i] = FREE;
                        whitekingcapture(b, &n, movelist, i + 10);
                        b[i + 5] = tmp;
                        b[i] = CB_WHITE | CB_KING;
                    }
                    if ((b[i - 4] & CB_BLACK) != 0 && (b[i - 8] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 8);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 4];
                        m = (m << 8) + (i - 4);
                        movelist[n].m[2] = m;
                        int tmp = b[i - 4];
                        b[i - 4] = FREE;
                        b[i] = FREE;
                        whitekingcapture(b, &n, movelist, i - 8);
                        b[i - 4] = tmp;
                        b[i] = CB_WHITE | CB_KING;
                    }
                    if ((b[i - 5] & CB_BLACK) != 0 && (b[i - 10] & FREE) != 0) {
                        movelist[n].n = 3;
                        m = ((CB_WHITE | CB_KING) << 8) + FREE;
                        m = (m << 8) + (i - 10);
                        movelist[n].m[1] = m;
                        m = (FREE << 8) + (CB_WHITE | CB_KING);
                        m = (m << 8) + i;
                        movelist[n].m[0] = m;
                        m = (FREE << 8) + b[i - 5];
                        m = (m << 8) + (i - 5);
                        movelist[n].m[2] = m;
                        int tmp = b[i - 5];
                        b[i - 5] = FREE;
                        b[i] = FREE;
                        whitekingcapture(b, &n, movelist, i - 10);
                        b[i - 5] = tmp;
                        b[i] = CB_WHITE | CB_KING;
                    }
                }
            }
        }
    }

    // Apply Italian Checkers Law of Maximum
    // 1. Capture maximum number of pieces
    for (int i = 0; i < n; i++) ismove[i] = 1;
    int max = 0;
    for (int i = 0; i < n; i++) {
        if (movelist[i].n > max) max = movelist[i].n;
    }
    for (int i = 0; i < n; i++) {
        if (movelist[i].n < max) ismove[i] = 0;
    }

    // 2. Capture with king if possible
    max = 0;
    for (int i = 0; i < n; i++) {
        if (!ismove[i]) continue;
        if (((movelist[i].m[0] >> 8) % 256) & CB_KING) max = 1;
    }
    if (max == 1) {
        for (int i = 0; i < n; i++) {
            if (!(((movelist[i].m[0] >> 8) % 256) & CB_KING)) ismove[i] = 0;
        }
    }

    // 3. Capture maximum number of kings
    max = 0;
    for (int i = 0; i < n; i++) {
        if (!ismove[i]) continue;
        int tmp = 0;
        for (int j = 2; j < movelist[i].n; j++) {
            if (((movelist[i].m[j] >> 8) % 256) & CB_KING) tmp++;
        }
        if (tmp > max) max = tmp;
    }
    if (max > 0) {
        for (int i = 0; i < n; i++) {
            if (!ismove[i]) continue;
            int tmp = 0;
            for (int j = 2; j < movelist[i].n; j++) {
                if (((movelist[i].m[j] >> 8) % 256) & CB_KING) tmp++;
            }
            if (tmp < max) ismove[i] = 0;
        }
    }

    // 4. Capture king earliest in sequence
    max = 0;
    for (int i = 0; i < n; i++) {
        if (!ismove[i]) continue;
        for (int j = 2; j < movelist[i].n; j++) {
            if (((movelist[i].m[j] >> 8) % 256) & CB_KING) {
                if (j > max) max = j;
                break;
            }
        }
    }
    if (max > 0) {
        for (int i = 0; i < n; i++) {
            if (!ismove[i]) continue;
            for (int j = 2; j < movelist[i].n; j++) {
                if (((movelist[i].m[j] >> 8) % 256) & CB_KING) {
                    if (j > max) ismove[i] = 0;
                    break;
                }
            }
        }
    }

    // Compact list
    int n2 = 0;
    for (int i = 0; i < n; i++) {
        if (ismove[i]) {
            movelist[n2++] = movelist[i];
        }
    }
    return n2;
}

static int testcapture(int b[46], int color) {
    if (color == CB_BLACK) {
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_BLACK) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if (b[i + 4] == (CB_WHITE | CB_MAN) && (b[i + 8] & FREE) != 0) return 1;
                    if (b[i + 5] == (CB_WHITE | CB_MAN) && (b[i + 10] & FREE) != 0) return 1;
                } else {
                    if ((b[i + 4] & CB_WHITE) != 0 && (b[i + 8] & FREE) != 0) return 1;
                    if ((b[i + 5] & CB_WHITE) != 0 && (b[i + 10] & FREE) != 0) return 1;
                    if ((b[i - 4] & CB_WHITE) != 0 && (b[i - 8] & FREE) != 0) return 1;
                    if ((b[i - 5] & CB_WHITE) != 0 && (b[i - 10] & FREE) != 0) return 1;
                }
            }
        }
    } else {
        for (int i = 5; i <= 40; i++) {
            if ((b[i] & CB_WHITE) != 0) {
                if ((b[i] & CB_MAN) != 0) {
                    if (b[i - 4] == (CB_BLACK | CB_MAN) && (b[i - 8] & FREE) != 0) return 1;
                    if (b[i - 5] == (CB_BLACK | CB_MAN) && (b[i - 10] & FREE) != 0) return 1;
                } else {
                    if ((b[i + 4] & CB_BLACK) != 0 && (b[i + 8] & FREE) != 0) return 1;
                    if ((b[i + 5] & CB_BLACK) != 0 && (b[i + 10] & FREE) != 0) return 1;
                    if ((b[i - 4] & CB_BLACK) != 0 && (b[i - 8] & FREE) != 0) return 1;
                    if ((b[i - 5] & CB_BLACK) != 0 && (b[i - 10] & FREE) != 0) return 1;
                }
            }
        }
    }
    return 0;
}

// Engine lifecycle & Move bridge for Damascus
void engine_checkerboard_init(void **state) {
    CheckerboardEngineState *st = (CheckerboardEngineState*)malloc(sizeof(CheckerboardEngineState));
    if (st) {
        st->search_time = 0.60; // 600ms search per move
    }
    *state = st;
}

Move engine_checkerboard_get_move(void *state, const GameState *game) {
    if (!game || game->is_game_over) return MOVE_NONE;

    CheckerboardEngineState *st = (CheckerboardEngineState*)state;
    double search_time = st ? st->search_time : 0.60;

    // Convert Damascus bitboard to CheckerBoard board[46]
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

    int board[46];
    for (int i = 0; i < 46; i++) board[i] = OCCUPIED;
    for (int i = 5; i <= 40; i++) board[i] = FREE;

    board[5]  = b[0][0]; board[6]  = b[2][0]; board[7]  = b[4][0]; board[8]  = b[6][0];
    board[10] = b[1][1]; board[11] = b[3][1]; board[12] = b[5][1]; board[13] = b[7][1];
    board[14] = b[0][2]; board[15] = b[2][2]; board[16] = b[4][2]; board[17] = b[6][2];
    board[19] = b[1][3]; board[20] = b[3][3]; board[21] = b[5][3]; board[22] = b[7][3];
    board[23] = b[0][4]; board[24] = b[2][4]; board[25] = b[4][4]; board[26] = b[6][4];
    board[28] = b[1][5]; board[29] = b[3][5]; board[30] = b[5][5]; board[31] = b[7][5];
    board[32] = b[0][6]; board[33] = b[2][6]; board[34] = b[4][6]; board[35] = b[6][6];
    board[37] = b[1][7]; board[38] = b[3][7]; board[39] = b[5][7]; board[40] = b[7][7];

    for (int i = 5; i <= 40; i++) {
        if (board[i] == 0) board[i] = FREE;
    }
    for (int i = 9; i <= 36; i += 9) {
        board[i] = OCCUPIED;
    }

    int cb_color = (game->current_player == PLAYER_WHITE) ? CB_WHITE : CB_BLACK;

    memset(&g_best_cb_move, 0, sizeof(g_best_cb_move));
    checkers_search(board, cb_color, search_time);

    // Convert CB best move to Damascus coordinates
    int from_row = 7 - g_best_cb_move.from.y;
    int from_col = 7 - g_best_cb_move.from.x;
    int to_row = 7 - g_best_cb_move.to.y;
    int to_col = 7 - g_best_cb_move.to.x;

    int from_sq = ROW_COL_TO_SQ(from_row, from_col);
    int to_sq = ROW_COL_TO_SQ(to_row, to_col);
    int jumps = g_best_cb_move.jumps;

    // Match with valid Damascus moves
    const MoveList *valid_moves = game_get_valid_moves(game);

    // 1. Exact match on from, to, and jump count
    for (int i = 0; i < valid_moves->count; i++) {
        Move m = valid_moves->moves[i];
        if (MOVE_FROM(m) == from_sq && MOVE_TO(m) == to_sq && m.jumps == jumps) {
            return m;
        }
    }

    // 2. Match on from and to
    for (int i = 0; i < valid_moves->count; i++) {
        Move m = valid_moves->moves[i];
        if (MOVE_FROM(m) == from_sq && MOVE_TO(m) == to_sq) {
            return m;
        }
    }

    // 3. Match on from
    for (int i = 0; i < valid_moves->count; i++) {
        Move m = valid_moves->moves[i];
        if (MOVE_FROM(m) == from_sq) {
            return m;
        }
    }

    // 4. Fallback to first valid move if any
    if (valid_moves->count > 0) return valid_moves->moves[0];
    return MOVE_NONE;
}

void engine_checkerboard_cleanup(void *state) {
    if (state) free(state);
}

void engine_checkerboard_set_search_time(void *state, double seconds) {
    if (state && seconds > 0.0) {
        ((CheckerboardEngineState*)state)->search_time = seconds;
    }
}

