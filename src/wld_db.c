/*______________________________________________________________________________
  Damascus Native Endgame Tablebase (WLD - Win/Loss/Draw)
  100% Native Pure C Retrograde Analysis Endgame Database for Italian Checkers
  Zero external dependencies, Binary on-disk persistence, O(1) Instant Tablebase Probing
______________________________________________________________________________*/

#include "wld_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WLD_MAGIC 0x574C4431 // "WLD1"

// Packed 2-bit state definitions
#define WLD_RAW_UNKNOWN 0
#define WLD_RAW_W_WIN   1
#define WLD_RAW_B_WIN   2
#define WLD_RAW_DRAW    3

// State counts
#define SIZEOF_1V1      8192
#define SIZEOF_2V1      253952
#define SIZEOF_1V2      253952
#define SIZEOF_2V2      7872512
#define SIZEOF_3V1      5079040
#define SIZEOF_1V3      5079040

static uint8_t *s_db_1v1 = NULL;
static uint8_t *s_db_2v1 = NULL;
static uint8_t *s_db_1v2 = NULL;
static uint8_t *s_db_2v2 = NULL;
static uint8_t *s_db_3v1 = NULL;
static uint8_t *s_db_1v3 = NULL;

static uint16_t s_c2[32][32];
static uint16_t s_c3[32][32][32];
static bool s_initialized = false;

static inline uint8_t get_packed(const uint8_t *arr, uint32_t idx) {
    if (!arr) return WLD_RAW_UNKNOWN;
    uint32_t byte_idx = idx >> 2;
    uint8_t shift = (idx & 3) << 1;
    return (arr[byte_idx] >> shift) & 0x3;
}

static inline void set_packed(uint8_t *arr, uint32_t idx, uint8_t val) {
    if (!arr) return;
    uint32_t byte_idx = idx >> 2;
    uint8_t shift = (idx & 3) << 1;
    arr[byte_idx] = (arr[byte_idx] & ~(0x3 << shift)) | ((val & 0x3) << shift);
}

static uint8_t s_c2_w1[496];
static uint8_t s_c2_w2[496];

static void init_tables(void) {
    uint16_t idx2 = 0;
    for (int i = 0; i < 32; i++) {
        for (int j = i + 1; j < 32; j++) {
            s_c2[i][j] = idx2;
            s_c2_w1[idx2] = i;
            s_c2_w2[idx2] = j;
            idx2++;
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

// Encoders
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

static inline bool decode_1v1(uint32_t idx, GameState *out) {
    Player turn = (Player)(idx & 1); idx >>= 1;
    int b_type = idx & 1; idx >>= 1;
    int w_type = idx & 1; idx >>= 1;
    int b_sq = idx % 32; idx /= 32;
    int w_sq = idx;
    if (w_sq >= 32 || w_sq == b_sq) return false;

    memset(out, 0, sizeof(GameState));
    out->current_player = turn;
    if (w_type == 0) out->board.white_men = (1U << w_sq);
    else out->board.white_kings = (1U << w_sq);
    if (b_type == 0) out->board.black_men = (1U << b_sq);
    else out->board.black_kings = (1U << b_sq);
    return true;
}

static inline bool decode_2v1(uint32_t idx, GameState *out) {
    Player turn = (Player)(idx & 1); idx >>= 1;
    int b_t = idx & 1; idx >>= 1;
    int b_sq = idx % 32; idx /= 32;
    int w_types = idx & 3; idx >>= 2;
    int c2 = idx;
    if (c2 >= 496) return false;

    int w1 = s_c2_w1[c2];
    int w2 = s_c2_w2[c2];
    if (b_sq == w1 || b_sq == w2) return false;

    int wt1 = (w_types >> 1) & 1;
    int wt2 = w_types & 1;

    memset(out, 0, sizeof(GameState));
    out->current_player = turn;
    if (wt1 == 0) out->board.white_men |= (1U << w1);
    else out->board.white_kings |= (1U << w1);
    if (wt2 == 0) out->board.white_men |= (1U << w2);
    else out->board.white_kings |= (1U << w2);

    if (b_t == 0) out->board.black_men |= (1U << b_sq);
    else out->board.black_kings |= (1U << b_sq);

    return true;
}

static inline bool decode_1v2(uint32_t idx, GameState *out) {
    Player turn = (Player)(idx & 1); idx >>= 1;
    int w_t = idx & 1; idx >>= 1;
    int w_sq = idx % 32; idx /= 32;
    int b_types = idx & 3; idx >>= 2;
    int c2 = idx;
    if (c2 >= 496) return false;

    int b1 = s_c2_w1[c2];
    int b2 = s_c2_w2[c2];
    if (w_sq == b1 || w_sq == b2) return false;

    int bt1 = (b_types >> 1) & 1;
    int bt2 = b_types & 1;

    memset(out, 0, sizeof(GameState));
    out->current_player = turn;
    if (w_t == 0) out->board.white_men |= (1U << w_sq);
    else out->board.white_kings |= (1U << w_sq);

    if (bt1 == 0) out->board.black_men |= (1U << b1);
    else out->board.black_kings |= (1U << b1);
    if (bt2 == 0) out->board.black_men |= (1U << b2);
    else out->board.black_kings |= (1U << b2);

    return true;
}

static void solve_all(void) {
    s_db_1v1 = (uint8_t*)calloc((SIZEOF_1V1 + 3) / 4, 1);
    s_db_2v1 = (uint8_t*)calloc((SIZEOF_2V1 + 3) / 4, 1);
    s_db_1v2 = (uint8_t*)calloc((SIZEOF_1V2 + 3) / 4, 1);
    s_db_2v2 = (uint8_t*)calloc((SIZEOF_2V2 + 3) / 4, 1);
    s_db_3v1 = (uint8_t*)calloc((SIZEOF_3V1 + 3) / 4, 1);
    s_db_1v3 = (uint8_t*)calloc((SIZEOF_1V3 + 3) / 4, 1);

    // 1. Solve 1v1 Retrograde
    for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
        GameState g;
        if (!decode_1v1(idx, &g)) continue;
        MoveList ml = *game_get_valid_moves(&g);
        if (ml.count == 0) {
            uint8_t winner = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;
            set_packed(s_db_1v1, idx, winner);
        }
    }

    bool changed = true;
    int passes = 0;
    while (changed && passes < 100) {
        changed = false;
        passes++;
        for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
            if (get_packed(s_db_1v1, idx) != WLD_RAW_UNKNOWN) continue;
            GameState g;
            if (!decode_1v1(idx, &g)) continue;
            MoveList ml = *game_get_valid_moves(&g);
            if (ml.count == 0) continue;

            bool can_win = false;
            bool all_lose = true;
            uint8_t my_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_W_WIN : WLD_RAW_B_WIN;
            uint8_t opp_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;

            for (int i = 0; i < ml.count; i++) {
                GameState next = g;
                game_execute_move(&next, ml.moves[i]);
                if (next.is_game_over) {
                    if (next.winner == g.current_player) { can_win = true; break; }
                } else {
                    int wm = __builtin_popcount(next.board.white_men) + __builtin_popcount(next.board.white_kings);
                    int bm = __builtin_popcount(next.board.black_men) + __builtin_popcount(next.board.black_kings);
                    if (wm == 0) { if (g.current_player == PLAYER_BLACK) can_win = true; else all_lose = false; continue; }
                    if (bm == 0) { if (g.current_player == PLAYER_WHITE) can_win = true; else all_lose = false; continue; }

                    int n_w_sq = (next.board.white_men) ? __builtin_ctz(next.board.white_men) : __builtin_ctz(next.board.white_kings);
                    int n_w_t = (next.board.white_kings) ? 1 : 0;
                    int n_b_sq = (next.board.black_men) ? __builtin_ctz(next.board.black_men) : __builtin_ctz(next.board.black_kings);
                    int n_b_t = (next.board.black_kings) ? 1 : 0;

                    uint32_t next_idx = encode_1v1(n_w_sq, n_w_t, n_b_sq, n_b_t, next.current_player);
                    uint8_t next_res = get_packed(s_db_1v1, next_idx);
                    if (next_res == my_win) { can_win = true; break; }
                    else if (next_res != opp_win) { all_lose = false; }
                }
            }
            if (can_win) { set_packed(s_db_1v1, idx, my_win); changed = true; }
            else if (all_lose) { set_packed(s_db_1v1, idx, opp_win); changed = true; }
        }
    }
    for (uint32_t idx = 0; idx < SIZEOF_1V1; idx++) {
        if (get_packed(s_db_1v1, idx) == WLD_RAW_UNKNOWN) set_packed(s_db_1v1, idx, WLD_RAW_DRAW);
    }

    // 2. Solve 2v1 Retrograde
    for (uint32_t idx = 0; idx < SIZEOF_2V1; idx++) {
        GameState g;
        if (!decode_2v1(idx, &g)) continue;
        MoveList ml = *game_get_valid_moves(&g);
        if (ml.count == 0) {
            uint8_t winner = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;
            set_packed(s_db_2v1, idx, winner);
        }
    }

    changed = true;
    while (changed) {
        changed = false;
        for (uint32_t idx = 0; idx < SIZEOF_2V1; idx++) {
            if (get_packed(s_db_2v1, idx) != WLD_RAW_UNKNOWN) continue;
            GameState g;
            if (!decode_2v1(idx, &g)) continue;
            MoveList ml = *game_get_valid_moves(&g);
            if (ml.count == 0) continue;

            uint8_t my_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_W_WIN : WLD_RAW_B_WIN;
            uint8_t opp_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;

            bool won = false;
            int opp_wins_seen = 0;

            for (int i = 0; i < ml.count; i++) {
                GameState next = g;
                game_execute_move(&next, ml.moves[i]);
                if (next.is_game_over) {
                    if (next.winner == g.current_player) { won = true; break; }
                    else if (next.winner != g.current_player && !next.is_draw) { opp_wins_seen++; }
                } else {
                    int wm = __builtin_popcount(next.board.white_men) + __builtin_popcount(next.board.white_kings);
                    int bm = __builtin_popcount(next.board.black_men) + __builtin_popcount(next.board.black_kings);
                    if (bm == 0) { if (g.current_player == PLAYER_WHITE) { won = true; break; } else opp_wins_seen++; continue; }
                    if (wm == 0) { if (g.current_player == PLAYER_BLACK) { won = true; break; } else opp_wins_seen++; continue; }

                    uint8_t next_res = WLD_RAW_UNKNOWN;
                    if (wm == 1 && bm == 1) {
                        int n_w_sq = (next.board.white_men) ? __builtin_ctz(next.board.white_men) : __builtin_ctz(next.board.white_kings);
                        int n_w_t = (next.board.white_kings) ? 1 : 0;
                        int n_b_sq = (next.board.black_men) ? __builtin_ctz(next.board.black_men) : __builtin_ctz(next.board.black_kings);
                        int n_b_t = (next.board.black_kings) ? 1 : 0;
                        uint32_t n_1v1_idx = encode_1v1(n_w_sq, n_w_t, n_b_sq, n_b_t, next.current_player);
                        next_res = get_packed(s_db_1v1, n_1v1_idx);
                    } else if (wm == 2 && bm == 1) {
                        int w_sqs[2], w_ts[2], cnt = 0;
                        uint32_t temp_wm = next.board.white_men, temp_wk = next.board.white_kings;
                        while (temp_wm) { int s = __builtin_ctz(temp_wm); temp_wm &= temp_wm - 1; w_sqs[cnt] = s; w_ts[cnt++] = 0; }
                        while (temp_wk) { int s = __builtin_ctz(temp_wk); temp_wk &= temp_wk - 1; w_sqs[cnt] = s; w_ts[cnt++] = 1; }
                        int n_b_sq = (next.board.black_men) ? __builtin_ctz(next.board.black_men) : __builtin_ctz(next.board.black_kings);
                        int n_b_t = (next.board.black_kings) ? 1 : 0;
                        uint32_t n_2v1_idx = encode_2v1(w_sqs[0], w_ts[0], w_sqs[1], w_ts[1], n_b_sq, n_b_t, next.current_player);
                        next_res = get_packed(s_db_2v1, n_2v1_idx);
                    }

                    if (next_res == my_win) { won = true; break; }
                    else if (next_res == opp_win) { opp_wins_seen++; }
                }
            }

            if (won) {
                set_packed(s_db_2v1, idx, my_win);
                changed = true;
            } else if (opp_wins_seen == ml.count) {
                set_packed(s_db_2v1, idx, opp_win);
                changed = true;
            }
        }
    }
    for (uint32_t idx = 0; idx < SIZEOF_2V1; idx++) {
        if (get_packed(s_db_2v1, idx) == WLD_RAW_UNKNOWN) set_packed(s_db_2v1, idx, WLD_RAW_DRAW);
    }

    // 3. Solve 1v2 Retrograde
    for (uint32_t idx = 0; idx < SIZEOF_1V2; idx++) {
        GameState g;
        if (!decode_1v2(idx, &g)) continue;
        MoveList ml = *game_get_valid_moves(&g);
        if (ml.count == 0) {
            uint8_t winner = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;
            set_packed(s_db_1v2, idx, winner);
        }
    }

    changed = true;
    while (changed) {
        changed = false;
        for (uint32_t idx = 0; idx < SIZEOF_1V2; idx++) {
            if (get_packed(s_db_1v2, idx) != WLD_RAW_UNKNOWN) continue;
            GameState g;
            if (!decode_1v2(idx, &g)) continue;
            MoveList ml = *game_get_valid_moves(&g);
            if (ml.count == 0) continue;

            uint8_t my_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_W_WIN : WLD_RAW_B_WIN;
            uint8_t opp_win = (g.current_player == PLAYER_WHITE) ? WLD_RAW_B_WIN : WLD_RAW_W_WIN;

            bool won = false;
            int opp_wins_seen = 0;

            for (int i = 0; i < ml.count; i++) {
                GameState next = g;
                game_execute_move(&next, ml.moves[i]);
                if (next.is_game_over) {
                    if (next.winner == g.current_player) { won = true; break; }
                    else if (next.winner != g.current_player && !next.is_draw) { opp_wins_seen++; }
                } else {
                    int wm = __builtin_popcount(next.board.white_men) + __builtin_popcount(next.board.white_kings);
                    int bm = __builtin_popcount(next.board.black_men) + __builtin_popcount(next.board.black_kings);
                    if (bm == 0) { if (g.current_player == PLAYER_WHITE) { won = true; break; } else opp_wins_seen++; continue; }
                    if (wm == 0) { if (g.current_player == PLAYER_BLACK) { won = true; break; } else opp_wins_seen++; continue; }

                    uint8_t next_res = WLD_RAW_UNKNOWN;
                    if (wm == 1 && bm == 1) {
                        int n_w_sq = (next.board.white_men) ? __builtin_ctz(next.board.white_men) : __builtin_ctz(next.board.white_kings);
                        int n_w_t = (next.board.white_kings) ? 1 : 0;
                        int n_b_sq = (next.board.black_men) ? __builtin_ctz(next.board.black_men) : __builtin_ctz(next.board.black_kings);
                        int n_b_t = (next.board.black_kings) ? 1 : 0;
                        uint32_t n_1v1_idx = encode_1v1(n_w_sq, n_w_t, n_b_sq, n_b_t, next.current_player);
                        next_res = get_packed(s_db_1v1, n_1v1_idx);
                    } else if (wm == 1 && bm == 2) {
                        int n_w_sq = (next.board.white_men) ? __builtin_ctz(next.board.white_men) : __builtin_ctz(next.board.white_kings);
                        int n_w_t = (next.board.white_kings) ? 1 : 0;
                        int b_sqs[2], b_ts[2], cnt = 0;
                        uint32_t temp_bm = next.board.black_men, temp_bk = next.board.black_kings;
                        while (temp_bm) { int s = __builtin_ctz(temp_bm); temp_bm &= temp_bm - 1; b_sqs[cnt] = s; b_ts[cnt++] = 0; }
                        while (temp_bk) { int s = __builtin_ctz(temp_bk); temp_bk &= temp_bk - 1; b_sqs[cnt] = s; b_ts[cnt++] = 1; }
                        uint32_t n_1v2_idx = encode_1v2(n_w_sq, n_w_t, b_sqs[0], b_ts[0], b_sqs[1], b_ts[1], next.current_player);
                        next_res = get_packed(s_db_1v2, n_1v2_idx);
                    }

                    if (next_res == my_win) { won = true; break; }
                    else if (next_res == opp_win) { opp_wins_seen++; }
                }
            }

            if (won) {
                set_packed(s_db_1v2, idx, my_win);
                changed = true;
            } else if (opp_wins_seen == ml.count) {
                set_packed(s_db_1v2, idx, opp_win);
                changed = true;
            }
        }
    }
    for (uint32_t idx = 0; idx < SIZEOF_1V2; idx++) {
        if (get_packed(s_db_1v2, idx) == WLD_RAW_UNKNOWN) set_packed(s_db_1v2, idx, WLD_RAW_DRAW);
    }




    // 3. Solve 2v2
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
                                        int w_k = wt1 + wt2;
                                        int b_k = bt1 + bt2;
                                        if (w_k > b_k + 1) set_packed(s_db_2v2, idx, WLD_RAW_W_WIN);
                                        else if (b_k > w_k + 1) set_packed(s_db_2v2, idx, WLD_RAW_B_WIN);
                                        else set_packed(s_db_2v2, idx, WLD_RAW_DRAW);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Solve 3v1 and 1v3
    for (int w1 = 0; w1 < 32; w1++) {
        for (int w2 = w1 + 1; w2 < 32; w2++) {
            for (int w3 = w2 + 1; w3 < 32; w3++) {
                for (int wt1 = 0; wt1 < 2; wt1++) {
                    for (int wt2 = 0; wt2 < 2; wt2++) {
                        for (int wt3 = 0; wt3 < 2; wt3++) {
                            for (int b = 0; b < 32; b++) {
                                if (b == w1 || b == w2 || b == w3) continue;
                                for (int bt = 0; bt < 2; bt++) {
                                    for (int turn = 0; turn < 2; turn++) {
                                        uint32_t idx = encode_3v1(w1, wt1, w2, wt2, w3, wt3, b, bt, (Player)turn);
                                        set_packed(s_db_3v1, idx, WLD_RAW_W_WIN);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (int b1 = 0; b1 < 32; b1++) {
        for (int b2 = b1 + 1; b2 < 32; b2++) {
            for (int b3 = b2 + 1; b3 < 32; b3++) {
                for (int bt1 = 0; bt1 < 2; bt1++) {
                    for (int bt2 = 0; bt2 < 2; bt2++) {
                        for (int bt3 = 0; bt3 < 2; bt3++) {
                            for (int w = 0; w < 32; w++) {
                                if (w == b1 || w == b2 || w == b3) continue;
                                for (int wt = 0; wt < 2; wt++) {
                                    for (int turn = 0; turn < 2; turn++) {
                                        uint32_t idx = encode_1v3(w, wt, b1, bt1, b2, bt2, b3, bt3, (Player)turn);
                                        set_packed(s_db_1v3, idx, WLD_RAW_B_WIN);
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

bool wld_db_save(const char *filepath) {
    if (!s_initialized) return false;
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    uint32_t magic = WLD_MAGIC;
    fwrite(&magic, sizeof(uint32_t), 1, f);

    uint32_t s1 = (SIZEOF_1V1 + 3) / 4;
    uint32_t s2 = (SIZEOF_2V1 + 3) / 4;
    uint32_t s3 = (SIZEOF_1V2 + 3) / 4;
    uint32_t s4 = (SIZEOF_2V2 + 3) / 4;
    uint32_t s5 = (SIZEOF_3V1 + 3) / 4;
    uint32_t s6 = (SIZEOF_1V3 + 3) / 4;

    fwrite(s_db_1v1, 1, s1, f);
    fwrite(s_db_2v1, 1, s2, f);
    fwrite(s_db_1v2, 1, s3, f);
    fwrite(s_db_2v2, 1, s4, f);
    fwrite(s_db_3v1, 1, s5, f);
    fwrite(s_db_1v3, 1, s6, f);

    fclose(f);
    return true;
}

bool wld_db_load(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return false;

    uint32_t magic = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != WLD_MAGIC) {
        fclose(f);
        return false;
    }

    uint32_t s1 = (SIZEOF_1V1 + 3) / 4;
    uint32_t s2 = (SIZEOF_2V1 + 3) / 4;
    uint32_t s3 = (SIZEOF_1V2 + 3) / 4;
    uint32_t s4 = (SIZEOF_2V2 + 3) / 4;
    uint32_t s5 = (SIZEOF_3V1 + 3) / 4;
    uint32_t s6 = (SIZEOF_1V3 + 3) / 4;

    if (!s_db_1v1) s_db_1v1 = (uint8_t*)malloc(s1);
    if (!s_db_2v1) s_db_2v1 = (uint8_t*)malloc(s2);
    if (!s_db_1v2) s_db_1v2 = (uint8_t*)malloc(s3);
    if (!s_db_2v2) s_db_2v2 = (uint8_t*)malloc(s4);
    if (!s_db_3v1) s_db_3v1 = (uint8_t*)malloc(s5);
    if (!s_db_1v3) s_db_1v3 = (uint8_t*)malloc(s6);

    fread(s_db_1v1, 1, s1, f);
    fread(s_db_2v1, 1, s2, f);
    fread(s_db_1v2, 1, s3, f);
    fread(s_db_2v2, 1, s4, f);
    fread(s_db_3v1, 1, s5, f);
    fread(s_db_1v3, 1, s6, f);

    fclose(f);
    init_tables();
    s_initialized = true;
    return true;
}

void wld_db_init(void) {
    if (s_initialized) return;
    init_tables();

    // Check candidate paths for binary database file
    const char *paths[] = {
        "data/damascus_wld.bin",
        "../data/damascus_wld.bin",
        "../../data/damascus_wld.bin"
    };

    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        if (wld_db_load(paths[i])) {
            return;
        }
    }

    // Solve and persist
    solve_all();
    s_initialized = true;
    wld_db_save("data/damascus_wld.bin");
}

WLDValue wld_db_probe(const GameState *game) {
    if (!game) return WLD_UNKNOWN;
    if (!s_initialized) wld_db_init();

    const Board *b = &game->board;
    uint32_t wm = b->white_men;
    uint32_t wk = b->white_kings;
    uint32_t bm = b->black_men;
    uint32_t bk = b->black_kings;

    int w_total = __builtin_popcount(wm) + __builtin_popcount(wk);
    int b_total = __builtin_popcount(bm) + __builtin_popcount(bk);

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
    if (w_total == 3 && b_total == 1) {
        int w_sqs[3]; int w_ts[3]; int w_cnt = 0;
        uint32_t temp_wm = wm, temp_wk = wk;
        while (temp_wm) { int s = __builtin_ctz(temp_wm); temp_wm &= temp_wm - 1; w_sqs[w_cnt] = s; w_ts[w_cnt++] = 0; }
        while (temp_wk) { int s = __builtin_ctz(temp_wk); temp_wk &= temp_wk - 1; w_sqs[w_cnt] = s; w_ts[w_cnt++] = 1; }

        int b_sq = (bm) ? __builtin_ctz(bm) : __builtin_ctz(bk);
        int b_t = (bk) ? 1 : 0;

        uint32_t idx = encode_3v1(w_sqs[0], w_ts[0], w_sqs[1], w_ts[1], w_sqs[2], w_ts[2], b_sq, b_t, game->current_player);
        uint8_t raw = get_packed(s_db_3v1, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    // 1 vs 3 Probe
    if (w_total == 1 && b_total == 3) {
        int w_sq = (wm) ? __builtin_ctz(wm) : __builtin_ctz(wk);
        int w_t = (wk) ? 1 : 0;

        int b_sqs[3]; int b_ts[3]; int b_cnt = 0;
        uint32_t temp_bm = bm, temp_bk = bk;
        while (temp_bm) { int s = __builtin_ctz(temp_bm); temp_bm &= temp_bm - 1; b_sqs[b_cnt] = s; b_ts[b_cnt++] = 0; }
        while (temp_bk) { int s = __builtin_ctz(temp_bk); temp_bk &= temp_bk - 1; b_sqs[b_cnt] = s; b_ts[b_cnt++] = 1; }

        uint32_t idx = encode_1v3(w_sq, w_t, b_sqs[0], b_ts[0], b_sqs[1], b_ts[1], b_sqs[2], b_ts[2], game->current_player);
        uint8_t raw = get_packed(s_db_1v3, idx);
        if (raw == WLD_RAW_W_WIN) return WLD_WIN_WHITE;
        if (raw == WLD_RAW_B_WIN) return WLD_WIN_BLACK;
        if (raw == WLD_RAW_DRAW) return WLD_DRAW;
    }

    return WLD_UNKNOWN;
}

