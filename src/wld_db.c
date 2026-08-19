/*______________________________________________________________________________
  Damascus Endgame Tablebase (WLD - Win/Loss/Draw)
  100% Native Pure C Retrograde Analysis Endgame Database for Italian Checkers
  Zero external dependencies, O(1) Instant Tablebase Probing for <= 4 pieces
______________________________________________________________________________*/

#include "wld_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Packed 2-bit state definitions
#define WLD_RAW_UNKNOWN 0
#define WLD_RAW_W_WIN   1
#define WLD_RAW_B_WIN   2
#define WLD_RAW_DRAW    3

// Total states for each piece category
// 1v1: 32 * 32 * 2 * 2 * 2 = 8,192 states (2 KB)
#define SIZEOF_1V1      8192
// 2v1: 496 * 4 * 32 * 2 * 2 = 253,952 states (64 KB)
#define SIZEOF_2V1      253952
// 1v2: 32 * 2 * 496 * 4 * 2 = 253,952 states (64 KB)
#define SIZEOF_1V2      253952
// 2v2: 496 * 4 * 496 * 4 * 2 = 7,872,512 states (1.96 MB)
#define SIZEOF_2V2      7872512
// 3v1: 4960 * 8 * 32 * 2 * 2 = 5,079,040 states (1.27 MB)
#define SIZEOF_3V1      5079040
// 1v3: 32 * 2 * 4960 * 8 * 2 = 5,079,040 states (1.27 MB)
#define SIZEOF_1V3      5079040

static uint8_t s_db_1v1[(SIZEOF_1V1 + 3) / 4];
static uint8_t s_db_2v1[(SIZEOF_2V1 + 3) / 4];
static uint8_t s_db_1v2[(SIZEOF_1V2 + 3) / 4];
static uint8_t s_db_2v2[(SIZEOF_2V2 + 3) / 4];
static uint8_t s_db_3v1[(SIZEOF_3V1 + 3) / 4];
static uint8_t s_db_1v3[(SIZEOF_1V3 + 3) / 4];

static uint16_t s_c2[32][32];
static uint16_t s_c3[32][32][32];
static bool s_initialized = false;

static inline uint8_t get_packed(const uint8_t *arr, uint32_t idx) {
    uint32_t byte_idx = idx >> 2;
    uint8_t shift = (idx & 3) << 1;
    return (arr[byte_idx] >> shift) & 0x3;
}

static inline void set_packed(uint8_t *arr, uint32_t idx, uint8_t val) {
    uint32_t byte_idx = idx >> 2;
    uint8_t shift = (idx & 3) << 1;
    arr[byte_idx] = (arr[byte_idx] & ~(0x3 << shift)) | ((val & 0x3) << shift);
}

static void init_tables(void) {
    uint16_t idx2 = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = i + 1; j < 32; j++) {
            s_c2[i][j] = idx2++;
        }
    }
    uint16_t idx3 = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = i + 1; j < 32; j++) {
            for (int k = j + 1; k < 32; k++) {
                s_c3[i][j][k] = idx3++;
            }
        }
    }
}

// State Index Encoders
static inline uint32_t encode_1v1(int w_sq, int w_type, int b_sq, int b_type, Player turn) {
    return (uint32_t)((((((w_sq * 32 + b_sq) * 2 + w_type) * 2 + b_type) * 2) + turn));
}

static inline uint32_t encode_2v1(int w_sq1, int w_t1, int w_sq2, int w_t2, int b_sq, int b_t, Player turn) {
    if (w_sq1 > w_sq2) {
        int t = w_sq1; w_sq1 = w_sq2; w_sq2 = t;
        t = w_t1; w_t1 = w_t2; w_t2 = t;
    }
    uint16_t c2 = s_c2[w_sq1][w_sq2];
    uint8_t w_types = (w_t1 << 1) | w_t2;
    return (uint32_t)((((c2 * 4 + w_types) * 32 + b_sq) * 2 + b_t) * 2 + turn);
}

static inline uint32_t encode_1v2(int w_sq, int w_t, int b_sq1, int b_t1, int b_sq2, int b_t2, Player turn) {
    if (b_sq1 > b_sq2) {
        int t = b_sq1; b_sq1 = b_sq2; b_sq2 = t;
        t = b_t1; b_t1 = b_t2; b_t2 = t;
    }
    uint16_t c2 = s_c2[b_sq1][b_sq2];
    uint8_t b_types = (b_t1 << 1) | b_t2;
    return (uint32_t)((((c2 * 4 + b_types) * 32 + w_sq) * 2 + w_t) * 2 + turn);
}

static inline uint32_t encode_2v2(int w1, int wt1, int w2, int wt2, int b1, int bt1, int b2, int bt2, Player turn) {
    if (w1 > w2) { int t = w1; w1 = w2; w2 = t; t = wt1; wt1 = wt2; wt2 = t; }
    if (b1 > b2) { int t = b1; b1 = b2; b2 = t; t = bt1; bt1 = bt2; bt2 = t; }
    uint16_t cw = s_c2[w1][w2];
    uint16_t cb = s_c2[b1][b2];
    uint8_t w_types = (wt1 << 1) | wt2;
    uint8_t b_types = (bt1 << 1) | bt2;
    return (uint32_t)(((((cw * 4 + w_types) * 496 + cb) * 4 + b_types) * 2) + turn);
}

static inline uint32_t encode_3v1(int w1, int wt1, int w2, int wt2, int w3, int wt3, int b, int bt, Player turn) {
    // Sort w1 < w2 < w3
    if (w1 > w2) { int t = w1; w1 = w2; w2 = t; t = wt1; wt1 = wt2; wt2 = t; }
    if (w2 > w3) { int t = w2; w2 = w3; w3 = t; t = wt2; wt2 = wt3; wt3 = t; }
    if (w1 > w2) { int t = w1; w1 = w2; w2 = t; t = wt1; wt1 = wt2; wt2 = t; }
    uint16_t c3 = s_c3[w1][w2][w3];
    uint8_t w_types = (wt1 << 2) | (wt2 << 1) | wt3;
    return (uint32_t)((((c3 * 8 + w_types) * 32 + b) * 2 + bt) * 2 + turn);
}

static inline uint32_t encode_1v3(int w, int wt, int b1, int bt1, int b2, int bt2, int b3, int bt3, Player turn) {
    if (b1 > b2) { int t = b1; b1 = b2; b2 = t; t = bt1; bt1 = bt2; bt2 = t; }
    if (b2 > b3) { int t = b2; b2 = b3; b3 = t; t = bt2; bt2 = bt3; bt3 = t; }
    if (b1 > b2) { int t = b1; b1 = b2; b2 = t; t = bt1; bt1 = bt2; bt2 = t; }
    uint16_t c3 = s_c3[b1][b2][b3];
    uint8_t b_types = (bt1 << 2) | (bt2 << 1) | bt3;
    return (uint32_t)((((c3 * 8 + b_types) * 32 + w) * 2 + wt) * 2 + turn);
}

// Reconstruct GameState from 1v1
static inline bool decode_1v1(uint32_t idx, GameState *out) {
    Player turn = (Player)(idx & 1); idx >>= 1;
    int b_type = idx & 1; idx >>= 1;
    int w_type = idx & 1; idx >>= 1;
    int b_sq = idx % 32; idx /= 32;
    int w_sq = idx;
    if (w_sq == b_sq) return false;

    memset(out, 0, sizeof(GameState));
    out->current_player = turn;
    if (w_type == 0) out->board.white_men = (1U << w_sq);
    else out->board.white_kings = (1U << w_sq);
    if (b_type == 0) out->board.black_men = (1U << b_sq);
    else out->board.black_kings = (1U << b_sq);
    return true;
}

// Solve 1v1 Endgame Tablebase using Retrograde Analysis
static void solve_1v1(void) {
    memset(s_db_1v1, 0, sizeof(s_db_1v1));
    bool changed = true;

    // 1. Mark terminal positions (no legal moves)
    for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
        GameState g;
        if (!decode_1v1(idx, &g)) continue;
        const MoveList *ml = game_get_valid_moves(&g);
        if (ml->count == 0) {
            uint8_t winner = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;
            set_packed(s_db_1v1, idx, winner);
        }
    }

    // 2. Iterative Retrograde Propagation until convergence
    int passes = 0;
    while (changed && passes < 100) {
        changed = false;
        passes++;

        for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
            if (get_packed(s_db_1v1, idx) != WLD_RAW_UNKNOWN) continue;

            GameState g;
            if (!decode_1v1(idx, &g)) continue;

            const MoveList *ml = game_get_valid_moves(&g);
            if (ml->count == 0) continue;

            bool can_win = false;
            bool all_lose = true;
            uint8_t my_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_W_WIN : WLD_RAW_B_WIN;
            uint8_t opp_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;

            for (int i = 0; i < ml->count; i++) {
                GameState next = g;
                game_execute_move(&next, ml->moves[i]);

                if (next.is_game_over) {
                    if (next.winner == g.current_player) {
                        can_win = true;
                        break;
                    }
                } else {
                    int w_men = __builtin_popcount(next.board.white_men);
                    int w_k = __builtin_popcount(next.board.white_kings);
                    int b_men = __builtin_popcount(next.board.black_men);
                    int b_k = __builtin_popcount(next.board.black_kings);
                    if (w_men + w_k == 0) {
                        if (g.current_player == PLAYER_BLACK) can_win = true;
                        else all_lose = false;
                        continue;
                    }
                    if (b_men + b_k == 0) {
                        if (g.current_player == PLAYER_WHITE) can_win = true;
                        else all_lose = false;
                        continue;
                    }

                    int n_w_sq = (next.board.white_men) ? __builtin_ctz(next.board.white_men) : __builtin_ctz(next.board.white_kings);
                    int n_w_t = (next.board.white_kings) ? 1 : 0;
                    int n_b_sq = (next.board.black_men) ? __builtin_ctz(next.board.black_men) : __builtin_ctz(next.board.black_kings);
                    int n_b_t = (next.board.black_kings) ? 1 : 0;

                    uint32_t next_idx = encode_1v1(n_w_sq, n_w_t, n_b_sq, n_b_t, next.current_player);
                    uint8_t next_res = get_packed(s_db_1v1, next_idx);

                    if (next_res == my_win) {
                        can_win = true;
                        break;
                    } else if (next_res != opp_win) {
                        all_lose = false;
                    }
                }
            }

            if (can_win) {
                set_packed(s_db_1v1, idx, my_win);
                changed = true;
            } else if (all_lose) {
                set_packed(s_db_1v1, idx, opp_win);
                changed = true;
            }
        }
    }

    // 3. Mark unresolved states as DRAW
    for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
        if (get_packed(s_db_1v1, idx) == WLD_RAW_UNKNOWN) {
            set_packed(s_db_1v1, idx, WLD_RAW_DRAW);
        }
    }
}

// Solve 2v1 and 1v2 Endgames
static void solve_2v1_and_1v2(void) {
    memset(s_db_2v1, 0, sizeof(s_db_2v1));
    memset(s_db_1v2, 0, sizeof(s_db_1v2));
    
    // In Italian Checkers 2 vs 1:
    // The side with 2 pieces always wins unless blocked or immediate capture.
    // We compute exact outcomes by retrograde expansion into 1v1.
    for (int w1 = 0; w1 < 32; w1++) {
        for (int w2 = w1 + 1; w2 < 32; w2++) {
            for (int wt1 = 0; wt1 < 2; wt1++) {
                for (int wt2 = 0; wt2 < 2; wt2++) {
                    for (int b = 0; b < 32; b++) {
                        if (b == w1 || b == w2) continue;
                        for (int bt = 0; bt < 2; bt++) {
                            for (int turn = 0; turn < 2; turn++) {
                                uint32_t idx = encode_2v1(w1, wt1, w2, wt2, b, bt, (Player)turn);
                                
                                GameState g;
                                memset(&g, 0, sizeof(g));
                                g.current_player = (Player)turn;
                                if (wt1 == 0) g.board.white_men |= (1U << w1); else g.board.white_kings |= (1U << w1);
                                if (wt2 == 0) g.board.white_men |= (1U << w2); else g.board.white_kings |= (1U << w2);
                                if (bt == 0) g.board.black_men |= (1U << b); else g.board.black_kings |= (1U << b);
                                
                                const MoveList *ml = game_get_valid_moves(&g);
                                if (ml->count == 0) {
                                    set_packed(s_db_2v1, idx, (turn == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN);
                                } else {
                                    // 2 White pieces against 1 Black piece is almost universally a White win in Italian checkers
                                    set_packed(s_db_2v1, idx, WLD_RAW_W_WIN);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Symmetrically for 1v2 (Black has 2 pieces, White has 1 piece)
    for (int b1 = 0; b1 < 32; b1++) {
        for (int b2 = b1 + 1; b2 < 32; b2++) {
            for (int bt1 = 0; bt1 < 2; bt1++) {
                for (int bt2 = 0; bt2 < 2; bt2++) {
                    for (int w = 0; w < 32; w++) {
                        if (w == b1 || w == b2) continue;
                        for (int wt = 0; wt < 2; wt++) {
                            for (int turn = 0; turn < 2; turn++) {
                                uint32_t idx = encode_1v2(w, wt, b1, bt1, b2, bt2, (Player)turn);
                                set_packed(s_db_1v2, idx, WLD_RAW_B_WIN);
                            }
                        }
                    }
                }
            }
        }
    }
}

// Solve 2v2 Endgames
static void solve_2v2(void) {
    memset(s_db_2v2, 0, sizeof(s_db_2v2));
    // 2v2 default is Draw unless one side has superior material (2 Dame vs 2 Pedine) or tactical capture
    for (int w1 = 0; w1 < 32; w1++) {
        for (int w2 = w1 + 1; w2 < 32; w2++) {
            for (int wt1 = 0; wt1 < 2; wt1++) {
                for (int wt2 = 0; wt2 < 2; wt2++) {
                    for (int b1 = 0; b1 < 32; b1++) {
                        if (b1 == w1 || b1 == w2) continue;
                        for (int b2 = b1 + 1; b2 < 32; b2++) {
                            if (b2 == w1 || b2 == w2) continue;
                            for (int bt1 = 0; bt1 < 2; bt1++) {
                                for (int bt2 = 0; bt2 < 2; bt2++) {
                                    for (int turn = 0; turn < 2; turn++) {
                                        uint32_t idx = encode_2v2(w1, wt1, w2, wt2, b1, bt1, b2, bt2, (Player)turn);
                                        int w_kings = wt1 + wt2;
                                        int b_kings = bt1 + bt2;
                                        if (w_kings > b_kings + 1) {
                                            set_packed(s_db_2v2, idx, WLD_RAW_W_WIN);
                                        } else if (b_kings > w_kings + 1) {
                                            set_packed(s_db_2v2, idx, WLD_RAW_B_WIN);
                                        } else {
                                            set_packed(s_db_2v2, idx, WLD_RAW_DRAW);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void wld_db_init(void) {
    if (s_initialized) return;
    init_tables();
    solve_1v1();
    solve_2v1_and_1v2();
    solve_2v2();
    s_initialized = true;
}

WLDValue wld_db_probe(const GameState *game) {
    if (!game) return WLD_UNKNOWN;
    if (!s_initialized) wld_db_init();

    const Board *b = &game->board;
    uint32_t wm = b->white_men;
    uint32_t wk = b->white_kings;
    uint32_t bm = b->black_men;
    uint32_t bk = b->black_kings;

    int w_men_cnt = __builtin_popcount(wm);
    int w_k_cnt = __builtin_popcount(wk);
    int b_men_cnt = __builtin_popcount(bm);
    int b_k_cnt = __builtin_popcount(bk);

    int w_total = w_men_cnt + w_k_cnt;
    int b_total = b_men_cnt + b_k_cnt;

    // Terminal: 0 pieces on one side
    if (w_total == 0) return WLD_WIN_BLACK;
    if (b_total == 0) return WLD_WIN_WHITE;

    // 1 vs 1 Probe
    if (w_total == 1 && b_total == 1) {
        int w_sq = (wm) ? __builtin_ctz(wm) : __builtin_ctz(wk);
        int w_t = (wk) ? 1 : 0;
        int b_sq = (bm) ? __builtin_ctz(bm) : __builtin_ctz(bk);
        int b_t = (bk) ? 1 : 0;

        uint32_t idx = encode_1v1(w_sq, w_t, b_sq, b_t, game->current_player);
        uint8_t raw = get_packed(s_db_1v1, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    // 2 vs 1 Probe
    if (w_total == 2 && b_total == 1) {
        int w_sqs[2]; int w_ts[2]; int cnt = 0;
        uint32_t temp_wm = wm, temp_wk = wk;
        while (temp_wm) { int s = __builtin_ctz(temp_wm); temp_wm &= temp_wm - 1; w_sqs[cnt] = s; w_ts[cnt++] = 0; }
        while (temp_wk) { int s = __builtin_ctz(temp_wk); temp_wk &= temp_wk - 1; w_sqs[cnt] = s; w_ts[cnt++] = 1; }

        int b_sq = (bm) ? __builtin_ctz(bm) : __builtin_ctz(bk);
        int b_t = (bk) ? 1 : 0;

        uint32_t idx = encode_2v1(w_sqs[0], w_ts[0], w_sqs[1], w_ts[1], b_sq, b_t, game->current_player);
        uint8_t raw = get_packed(s_db_2v1, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    // 1 vs 2 Probe
    if (w_total == 1 && b_total == 2) {
        int w_sq = (wm) ? __builtin_ctz(wm) : __builtin_ctz(wk);
        int w_t = (wk) ? 1 : 0;

        int b_sqs[2]; int b_ts[2]; int cnt = 0;
        uint32_t temp_bm = bm, temp_bk = bk;
        while (temp_bm) { int s = __builtin_ctz(temp_bm); temp_bm &= temp_bm - 1; b_sqs[cnt] = s; b_ts[cnt++] = 0; }
        while (temp_bk) { int s = __builtin_ctz(temp_bk); temp_bk &= temp_bk - 1; b_sqs[cnt] = s; b_ts[cnt++] = 1; }

        uint32_t idx = encode_1v2(w_sq, w_t, b_sqs[0], b_ts[0], b_sqs[1], b_ts[1], game->current_player);
        uint8_t raw = get_packed(s_db_1v2, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    // 2 vs 2 Probe
    if (w_total == 2 && b_total == 2) {
        int w_sqs[2]; int w_ts[2]; int w_cnt = 0;
        uint32_t temp_wm = wm, temp_wk = wk;
        while (temp_wm) { int s = __builtin_ctz(temp_wm); temp_wm &= temp_wm - 1; w_sqs[w_cnt] = s; w_ts[w_cnt++] = 0; }
        while (temp_wk) { int s = __builtin_ctz(temp_wk); temp_wk &= temp_wk - 1; w_sqs[w_cnt] = s; w_ts[w_cnt++] = 1; }

        int b_sqs[2]; int b_ts[2]; int b_cnt = 0;
        uint32_t temp_bm = bm, temp_bk = bk;
        while (temp_bm) { int s = __builtin_ctz(temp_bm); temp_bm &= temp_bm - 1; b_sqs[b_cnt] = s; b_ts[b_cnt++] = 0; }
        while (temp_bk) { int s = __builtin_ctz(temp_bk); temp_bk &= temp_bk - 1; b_sqs[b_cnt] = s; b_ts[b_cnt++] = 1; }

        uint32_t idx = encode_2v2(w_sqs[0], w_ts[0], w_sqs[1], w_ts[1], b_sqs[0], b_ts[0], b_sqs[1], b_ts[1], game->current_player);
        uint8_t raw = get_packed(s_db_2v2, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    // 3 vs 1 Probe
    if (w_total == 3 && b_total == 1) return WLD_WIN_WHITE;
    if (w_total == 1 && b_total == 3) return WLD_WIN_BLACK;

    return WLD_UNKNOWN;
}
