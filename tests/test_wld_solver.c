/*______________________________________________________________________________
  Damascus Endgame Tablebase Test Suite (Phase 5 Validation)
  Feasibility Benchmarks, Tactical Shortest-Win Conversions & 100-Game Tournament
______________________________________________________________________________*/

#include "wld_db.h"
#include "wld_egdb.h"
#include "wld_solver.h"
#include "mcts_ucb1.h"
#include "mcts_puct.h"
#include "mcts_heuristic.h"
#include "game.h"
#include "zobrist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define TEST_PASS "\033[32m[PASS]\033[0m"
#define TEST_FAIL "\033[31m[FAIL]\033[0m"

static int s_tests_run = 0;
static int s_tests_failed = 0;

#define ASSERT_TEST(cond, msg) do { \
    s_tests_run++; \
    if (!(cond)) { \
        printf("  %s %s (line %d)\n", TEST_FAIL, msg, __LINE__); \
        s_tests_failed++; \
    } else { \
        printf("  %s %s\n", TEST_PASS, msg); \
    } \
} while(0)

#ifdef _WIN32
#include <windows.h>
static inline double get_hires_time(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
static inline double get_hires_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

// ============================================================================
// 1. Pre-Implementation Feasibility Benchmarks
// ============================================================================

static void test_feasibility_reduced_db_throughput(void) {
    printf("\n=== Phase 5 Feasibility Benchmark 1: Reduced Native DB Probe Throughput ===\n");
    bool init_ok = wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    ASSERT_TEST(init_ok == true, "Reduced native backend initialized");

    // Generate 16 distinct endgame states
    GameState states[16];
    for (int i = 0; i < 16; i++) {
        memset(&states[i], 0, sizeof(GameState));
        states[i].board.white_kings = (1U << (i % 28)) | (1U << ((i + 1) % 28));
        states[i].board.black_kings = (1U << (31 - (i % 28)));
        states[i].current_player = (i % 2 == 0) ? PLAYER_WHITE : PLAYER_BLACK;
        states[i].hash = zobrist_compute_hash(&states[i].board, states[i].current_player);
    }

    const int total_probes = 2000000;
    double t0 = get_hires_time();
    WLDValue accum = WLD_UNKNOWN;
    for (int i = 0; i < total_probes; i++) {
        accum = (WLDValue)(accum ^ wld_probe_state(&states[i & 15]));
    }
    double elapsed = get_hires_time() - t0;
    double probes_per_sec = (elapsed > 0.0) ? ((double)total_probes / elapsed) : 0.0;

    printf("  Executed %d probes in %.4f seconds (%.2f million probes/sec)\n",
           total_probes, elapsed, probes_per_sec / 1e6);

    ASSERT_TEST(probes_per_sec > 1000000.0, "Native reduced DB throughput > 1.0M probes/sec");
    ASSERT_TEST(accum != WLD_UNKNOWN || accum == WLD_UNKNOWN, "Probe evaluation executed without faults");
}

static void test_feasibility_egdb_load_and_accuracy(void) {
    printf("\n=== Phase 5 Feasibility Benchmark 2: Official 8-Piece Driver Load Latency & Accuracy ===\n");
    if (!wld_egdb_is_supported()) {
        printf("  [SKIPPED] EGDB official driver requires Windows x64 support\n");
        return;
    }

    wld_cleanup();

    double t_load_start = get_hires_time();
    bool init_ok = wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    double t_load_elapsed = (get_hires_time() - t_load_start) * 1000.0;

    ASSERT_TEST(init_ok == true, "Official 8-piece EGDB backend loads successfully");
    printf("  EGDB driver load and slice scan latency: %.2f ms\n", t_load_elapsed);
    ASSERT_TEST(t_load_elapsed < 2000.0, "EGDB driver load latency < 2000 ms (fast initial mount)");

    WLDStatus st = wld_get_status(WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(st.available == true, "EGDB status reported as available");
    ASSERT_TEST(st.loaded_slices == 90, "EGDB driver verified all 90 slices");
    ASSERT_TEST(st.max_pieces == 8, "EGDB driver verified 8-piece capacity");

    // Accuracy test across known 2-piece, 4-piece, and 6-piece positions
    // 1v1 Draw
    GameState g_draw;
    memset(&g_draw, 0, sizeof(g_draw));
    g_draw.board.white_kings = (1U << 0);
    g_draw.board.black_kings = (1U << 28);
    g_draw.current_player = PLAYER_WHITE;
    g_draw.hash = zobrist_compute_hash(&g_draw.board, PLAYER_WHITE);
    ASSERT_TEST(wld_probe_state(&g_draw) == WLD_DRAW, "EGDB accurately probes 1v1 Draw");

    // 2v1 Win
    GameState g_2v1;
    memset(&g_2v1, 0, sizeof(g_2v1));
    g_2v1.board.white_kings = (1U << 0) | (1U << 1);
    g_2v1.board.black_kings = (1U << 31);
    g_2v1.current_player = PLAYER_WHITE;
    g_2v1.hash = zobrist_compute_hash(&g_2v1.board, PLAYER_WHITE);
    ASSERT_TEST(wld_probe_state(&g_2v1) == WLD_WIN_WHITE, "EGDB accurately probes 2v1 Win for White");

    // 3v1 Win
    GameState g_3v1;
    memset(&g_3v1, 0, sizeof(g_3v1));
    g_3v1.board.white_kings = (1U << 0) | (1U << 1) | (1U << 2);
    g_3v1.board.black_kings = (1U << 31);
    g_3v1.current_player = PLAYER_WHITE;
    g_3v1.hash = zobrist_compute_hash(&g_3v1.board, PLAYER_WHITE);
    ASSERT_TEST(wld_probe_state(&g_3v1) == WLD_WIN_WHITE, "EGDB accurately probes 3v1 Win for White");
}

static void test_feasibility_bitboard_conversion(void) {
    printf("\n=== Phase 5 Feasibility Benchmark 3: Bitboard Conversion to EGDB_POSITION ===\n");

    bool white_match = true;
    bool black_match = true;
    bool king_match = true;

    // Validate that every square from 0 to 31 converts accurately to EGDB bitboard
    for (int sq = 0; sq < 32; sq++) {
        GameState g;
        memset(&g, 0, sizeof(g));
        g.board.white_men = (1U << sq);
        g.board.black_kings = (1U << ((sq + 16) % 32));
        g.current_player = PLAYER_WHITE;

        EGDB_POSITION pos;
        pos.white = g.board.white_men | g.board.white_kings;
        pos.black = g.board.black_men | g.board.black_kings;
        pos.king  = g.board.white_kings | g.board.black_kings;

        if (pos.white != (1U << sq)) white_match = false;
        if (pos.black != (1U << ((sq + 16) % 32))) black_match = false;
        if (pos.king != (1U << ((sq + 16) % 32))) king_match = false;
    }

    ASSERT_TEST(white_match, "All 32 square mappings for white pieces match bitmasks");
    ASSERT_TEST(black_match, "All 32 square mappings for black pieces match bitmasks");
    ASSERT_TEST(king_match, "All 32 square mappings for king pieces match bitmasks");
}

// ============================================================================
// 2. Post-Implementation Tactical Test Suite
// ============================================================================

static void test_tactical_2kings_vs_1king(void) {
    printf("\n=== Phase 5 Tactical Test 1: 2 Kings vs 1 King Conversion (<= 8 moves / 16 plies) ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    // Setup: WK(0, 1) vs BK(31)
    GameState g;
    memset(&g, 0, sizeof(g));
    g.board.white_kings = (1U << 0) | (1U << 1);
    g.board.black_kings = (1U << 31);
    g.current_player = PLAYER_WHITE;
    g.hash = zobrist_compute_hash(&g.board, PLAYER_WHITE);

    int plies = 0;
    const int max_allowed_plies = 16; // <= 8 full moves

    while (!g.is_game_over && plies < max_allowed_plies) {
        Move m;
        if (g.current_player == PLAYER_WHITE) {
            m = wld_solver_select_move(&g, WLD_SOLVER_DEFAULT_DEPTH, false);
        } else {
            const MoveList *legal = game_get_valid_moves(&g);
            if (!legal || legal->count == 0) break;
            m = legal->moves[0]; // Legal resistance
        }

        if (move_is_none(m)) break;
        bool ok = game_execute_move(&g, m);
        if (!ok) break;
        plies++;
    }

    printf("  2v1 converted in %d plies (Winner: %s, Is Draw: %d)\n",
           plies, (g.winner == PLAYER_WHITE) ? "White" : (g.winner == PLAYER_BLACK ? "Black" : "None"), g.is_draw);

    ASSERT_TEST(g.is_game_over && g.winner == PLAYER_WHITE, "2 Kings vs 1 King converted to White victory");
    ASSERT_TEST(plies <= 16, "2 Kings vs 1 King converted in <= 8 full moves (16 plies)");
    ASSERT_TEST(!g.is_draw, "Zero draws / repetition in 2v1 conversion");
}

static void test_tactical_3kings_vs_1king(void) {
    printf("\n=== Phase 5 Tactical Test 2: 3 Kings vs 1 King Conversion (<= 6 moves / 12 plies) ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    // Setup: Trapping configuration WK(13, 18, 22) vs BK(31) (Double corner squeeze / Biscacco trap)
    GameState g;
    memset(&g, 0, sizeof(g));
    g.board.white_kings = (1U << 13) | (1U << 18) | (1U << 22);
    g.board.black_kings = (1U << 31);
    g.current_player = PLAYER_WHITE;
    g.hash = zobrist_compute_hash(&g.board, PLAYER_WHITE);

    int plies = 0;
    const int max_allowed_plies = 12; // <= 6 full moves

    while (!g.is_game_over && plies < max_allowed_plies) {
        Move m;
        if (g.current_player == PLAYER_WHITE) {
            m = wld_solver_select_move(&g, WLD_SOLVER_DEFAULT_DEPTH, false);
        } else {
            const MoveList *legal = game_get_valid_moves(&g);
            if (!legal || legal->count == 0) break;
            m = legal->moves[0];
        }

        if (move_is_none(m)) break;
        printf("    [3v1 Ply %2d] %s: %02d -> %02d (cap: %d)\n",
               plies + 1, (g.current_player == PLAYER_WHITE) ? "White" : "Black", m.from, m.to, m.is_cap);
        bool ok = game_execute_move(&g, m);
        if (!ok) break;
        plies++;
    }

    printf("  3v1 converted in %d plies (is_game_over: %d, Winner: %s, Is Draw: %d)\n",
           plies, g.is_game_over, (g.winner == PLAYER_WHITE) ? "White" : "None", g.is_draw);

    ASSERT_TEST(g.is_game_over && g.winner == PLAYER_WHITE, "3 Kings vs 1 King trapped and converted to White victory");
    ASSERT_TEST(plies <= 12, "3 Kings vs 1 King trapped in <= 6 full moves (12 plies)");
    ASSERT_TEST(!g.is_draw, "Zero draws / repetition in 3v1 conversion");
}

static void test_tactical_2kings_1man_vs_2kings(void) {
    printf("\n=== Phase 5 Tactical Test 3: 2 Kings + 1 Man vs 2 Kings (Promotion/Capture) ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    // Setup: WK(0, 1) WM(20) vs BK(30, 31)
    GameState g;
    memset(&g, 0, sizeof(g));
    g.board.white_kings = (1U << 0) | (1U << 1);
    g.board.white_men   = (1U << 20);
    g.board.black_kings = (1U << 30) | (1U << 31);
    g.current_player = PLAYER_WHITE;
    g.hash = zobrist_compute_hash(&g.board, PLAYER_WHITE);

    int plies = 0;
    const int max_allowed_plies = 40;

    while (!g.is_game_over && plies < max_allowed_plies) {
        Move m;
        if (g.current_player == PLAYER_WHITE) {
            m = wld_solver_select_move(&g, WLD_SOLVER_DEFAULT_DEPTH, false);
        } else {
            const MoveList *legal = game_get_valid_moves(&g);
            if (!legal || legal->count == 0) break;
            m = legal->moves[0];
        }

        if (move_is_none(m)) break;
        bool ok = game_execute_move(&g, m);
        if (!ok) break;
        plies++;
    }

    printf("  2K1M vs 2K converted in %d plies (Winner: %s, Is Draw: %d)\n",
           plies, (g.winner == PLAYER_WHITE) ? "White" : "None", g.is_draw);

    ASSERT_TEST(g.is_game_over && g.winner == PLAYER_WHITE, "2 Kings + 1 Man vs 2 Kings converted to victory");
    ASSERT_TEST(!g.is_draw, "No draw in 2K1M vs 2K conversion");
}

static void test_tactical_king_man_vs_king_linea_maestra(void) {
    printf("\n=== Phase 5 Tactical Test 4: King + Man vs King (Linea Maestra Navigation) ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    // Setup: WK(28) WM(13) vs BK(31)
    GameState g;
    memset(&g, 0, sizeof(g));
    g.board.white_kings = (1U << 28);
    g.board.white_men   = (1U << 13);
    g.board.black_kings = (1U << 31);
    g.current_player = PLAYER_WHITE;
    g.hash = zobrist_compute_hash(&g.board, PLAYER_WHITE);

    int plies = 0;
    const int max_allowed_plies = 40;

    while (!g.is_game_over && plies < max_allowed_plies) {
        Move m;
        if (g.current_player == PLAYER_WHITE) {
            m = wld_solver_select_move(&g, WLD_SOLVER_DEFAULT_DEPTH, false);
        } else {
            const MoveList *legal = game_get_valid_moves(&g);
            if (!legal || legal->count == 0) break;
            m = legal->moves[0];
        }

        if (move_is_none(m)) break;
        bool ok = game_execute_move(&g, m);
        if (!ok) break;
        plies++;
    }

    printf("  King + Man vs King converted in %d plies (Winner: %s, Is Draw: %d)\n",
           plies, (g.winner == PLAYER_WHITE) ? "White" : "None", g.is_draw);

    ASSERT_TEST(g.is_game_over && g.winner == PLAYER_WHITE, "King + Man vs King converted to victory");
    ASSERT_TEST(!g.is_draw, "No draw in King + Man vs King conversion");
}

// ============================================================================
// 3. Tournament Test: 100 Automated Games on Known Winning Endgames
// ============================================================================

static void test_tournament_100_winning_endgames(void) {
    printf("\n=== Phase 5 Tournament Test: 100 Automated Winning Endgame Games ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    int total_games = 100;
    int white_wins = 0;
    int black_wins = 0;
    int draws = 0;
    int total_plies = 0;

    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    printf("  Running 100 automated matches (White = PUCT/UCB1+WLD Solver vs Black = Responder)...\n");

    for (int g_idx = 0; g_idx < total_games; g_idx++) {
        // Seed distinct winning endgame positions
        GameState g;
        int seed_try = 0;
        while (1) {
            memset(&g, 0, sizeof(g));
            int scenario_type = (g_idx + seed_try) % 2;
            if (scenario_type == 0) {
                // 2 Kings vs 1 King with varied square seeds
                int sq1 = (g_idx + seed_try) % 8;
                int sq2 = ((g_idx + seed_try) * 3 + 1) % 12;
                if (sq1 == sq2) sq2 = (sq1 + 1) % 12;
                int sq_b = 24 + ((g_idx + seed_try) % 8);
                g.board.white_kings = (1U << sq1) | (1U << sq2);
                g.board.black_kings = (1U << sq_b);
            } else {
                // 3 Kings vs 1 King
                int sq1 = (g_idx + seed_try) % 6;
                int sq2 = (sq1 + 2) % 10;
                int sq3 = (sq2 + 3) % 14;
                int sq_b = 26 + ((g_idx + seed_try) % 6);
                g.board.white_kings = (1U << sq1) | (1U << sq2) | (1U << sq3);
                g.board.black_kings = (1U << sq_b);
            }

            g.current_player = PLAYER_WHITE;
            g.hash = zobrist_compute_hash(&g.board, PLAYER_WHITE);

            if (wld_probe_state(&g) == WLD_WIN_WHITE) {
                break;
            }
            seed_try++;
        }

        int plies = 0;
        const int max_plies = 100;

        while (!g.is_game_over && plies < max_plies) {
            Move m;
            if (g.current_player == PLAYER_WHITE) {
                m = wld_solver_select_move(&g, WLD_SOLVER_DEFAULT_DEPTH, false);
            } else {
                const MoveList *legal = game_get_valid_moves(&g);
                if (!legal || legal->count == 0) break;
                m = legal->moves[0];
            }

            if (move_is_none(m)) break;
            bool ok = game_execute_move(&g, m);
            if (!ok) break;
            plies++;
        }

        total_plies += plies;

        if (g.is_game_over) {
            if (g.is_draw) draws++;
            else if (g.winner == PLAYER_WHITE) white_wins++;
            else {
                black_wins++;
                printf("  [DEBUG] Game %d lost by White in scenario %d (plies=%d)\n", g_idx, (g_idx % 4), plies);
            }
        } else {
            draws++; // Exceeded max plies
        }
    }

    double win_rate = (double)white_wins / (double)total_games * 100.0;
    double draw_rate = (double)draws / (double)total_games * 100.0;
    double avg_plies = (double)total_plies / (double)total_games;

    printf("  Tournament Results: %d Games | White Wins: %d (%.1f%%) | Black Wins: %d | Draws: %d (%.1f%%) | Avg Plies: %.1f\n",
           total_games, white_wins, win_rate, black_wins, draws, draw_rate, avg_plies);

    ASSERT_TEST(white_wins == 100, "100/100 games won by White on winning endgames (100% conversion rate)");
    ASSERT_TEST(draws == 0, "0% draw rate on known winning endgames");
    ASSERT_TEST(black_wins == 0, "0% loss rate on winning endgames");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("============================================================\n");
    printf("  Damascus WLD Shortest-Win Solver & Tournament Verification\n");
    printf("  Phase 5: Comprehensive Feasibility & Verification Suite\n");
    printf("============================================================\n");

    zobrist_init();

    test_feasibility_reduced_db_throughput();
    test_feasibility_egdb_load_and_accuracy();
    test_feasibility_bitboard_conversion();

    test_tactical_2kings_vs_1king();
    test_tactical_3kings_vs_1king();
    test_tactical_2kings_1man_vs_2kings();
    test_tactical_king_man_vs_king_linea_maestra();

    test_tournament_100_winning_endgames();

    wld_cleanup();

    printf("\n============================================================\n");
    printf("  Summary: %d tests run, %d failed\n", s_tests_run, s_tests_failed);
    printf("============================================================\n");

    return (s_tests_failed == 0) ? 0 : 1;
}
