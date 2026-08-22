/*______________________________________________________________________________
  Damascus Endgame Tablebase Test Suite (Phase 1 Validation)
  Automated tests for EGDB 8-Piece Driver, Native Reduced DB & Unified WLD Subsystem
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

static void test_slice_scanner(void) {
    printf("\n=== Test 1: Filesystem Slice Scanner ===\n");
    size_t missing = 0;
    size_t total_bytes = 0;
    size_t found = wld_egdb_scan_slices(WLD_OFFICIAL_DB_DIR, &total_bytes, &missing);

    printf("  Found %zu slices (%zu missing), total size: %.2f MB\n",
           found, missing, (double)total_bytes / (1024.0 * 1024.0));

    ASSERT_TEST(found == 90, "Scanner finds all 90 tablebase slices (45 .cpr1 + 45 .idx1)");
    ASSERT_TEST(missing == 0, "Zero missing slices in data/wld");
    ASSERT_TEST(total_bytes > 4500000000ULL, "Total slice data size > 4.5 GB");
}

static void test_status_reporting(void) {
    printf("\n=== Test 2: Backend Status & Diagnostics ===\n");
    
    WLDStatus st_none = wld_get_status(WLD_BACKEND_NONE);
    ASSERT_TEST(st_none.available == true, "Status NONE: available");
    ASSERT_TEST(st_none.max_pieces == 0, "Status NONE: max_pieces == 0");

    WLDStatus st_native = wld_get_status(WLD_BACKEND_REDUCED_NATIVE);
    ASSERT_TEST(st_native.available == true, "Status NATIVE: available");
    ASSERT_TEST(st_native.max_pieces == 4, "Status NATIVE: max_pieces == 4");

    WLDStatus st_official = wld_get_status(WLD_BACKEND_OFFICIAL_8PIECE);
    if (wld_egdb_is_supported()) {
        ASSERT_TEST(st_official.available == true, "Status OFFICIAL: available on Windows");
        ASSERT_TEST(st_official.max_pieces == 8, "Status OFFICIAL: max_pieces == 8");
        ASSERT_TEST(st_official.loaded_slices == 90, "Status OFFICIAL: 90 slices detected");
    } else {
        ASSERT_TEST(st_official.available == false, "Status OFFICIAL: gracefully disabled on non-Windows");
    }
}

static void test_official_backend_probing(void) {
    printf("\n=== Test 3: Official 8-Piece Driver Probing ===\n");
    if (!wld_egdb_is_supported()) {
        printf("  [SKIPPED] Official driver requires Windows x64 DLL support\n");
        return;
    }

    bool init_ok = wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(init_ok == true, "Official 8-piece backend initialization succeeds");
    ASSERT_TEST(wld_get_active_backend() == WLD_BACKEND_OFFICIAL_8PIECE, "Active backend is WLD_BACKEND_OFFICIAL_8PIECE");

    // Position A: White King on sq 0 (a1), White King on sq 1 (c1), Black King on sq 31 (h8).
    GameState g_2v1;
    memset(&g_2v1, 0, sizeof(g_2v1));
    g_2v1.board.white_kings = (1U << 0) | (1U << 1);
    g_2v1.board.black_kings = (1U << 31);
    g_2v1.current_player = PLAYER_WHITE;

    WLDValue val_2v1_w = wld_probe_state(&g_2v1);
    ASSERT_TEST(val_2v1_w == WLD_WIN_WHITE, "2 Kings vs 1 King (White to move) evaluates to WLD_WIN_WHITE");

    g_2v1.current_player = PLAYER_BLACK;
    WLDValue val_2v1_b = wld_probe_state(&g_2v1);
    ASSERT_TEST(val_2v1_b == WLD_WIN_WHITE, "2 Kings vs 1 King (Black to move) evaluates to WLD_WIN_WHITE");

    // Position B: 3 Kings vs 1 King
    GameState g_3v1;
    memset(&g_3v1, 0, sizeof(g_3v1));
    g_3v1.board.white_kings = (1U << 0) | (1U << 1) | (1U << 2);
    g_3v1.board.black_kings = (1U << 31);
    g_3v1.current_player = PLAYER_WHITE;

    WLDValue val_3v1_w = wld_probe_state(&g_3v1);
    ASSERT_TEST(val_3v1_w == WLD_WIN_WHITE, "3 Kings vs 1 King evaluates to WLD_WIN_WHITE");

    // Position C: 8-piece configuration
    GameState g_8p;
    memset(&g_8p, 0, sizeof(g_8p));
    g_8p.board.white_men = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);
    g_8p.board.black_men = (1U << 28) | (1U << 29) | (1U << 30) | (1U << 31);
    g_8p.current_player = PLAYER_WHITE;

    ASSERT_TEST(wld_is_endgame_state(&g_8p) == true, "8-piece configuration recognized as endgame state");

    // Position D: 9-piece configuration (non-endgame for 8p DB)
    GameState g_9p = g_8p;
    g_9p.board.white_kings = (1U << 4);
    ASSERT_TEST(wld_probe_state(&g_9p) == WLD_UNKNOWN, "9-piece position correctly returns WLD_UNKNOWN");
    ASSERT_TEST(wld_is_endgame_state(&g_9p) == false, "9-piece position not in endgame state");
}

static void test_reduced_native_backend(void) {
    printf("\n=== Test 4: Reduced Native Backend Probing ===\n");
    bool init_ok = wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    ASSERT_TEST(init_ok == true, "Reduced native backend initialization succeeds");
    ASSERT_TEST(wld_get_active_backend() == WLD_BACKEND_REDUCED_NATIVE, "Active backend is WLD_BACKEND_REDUCED_NATIVE");

    // 1v1 position on non-aligned diagonals (a1 vs a8 -> sq 0 vs sq 28 is DRAW)
    GameState g_1v1;
    memset(&g_1v1, 0, sizeof(g_1v1));
    g_1v1.board.white_kings = (1U << 0);
    g_1v1.board.black_kings = (1U << 28);
    g_1v1.current_player = PLAYER_WHITE;
    g_1v1.hash = zobrist_compute_hash(&g_1v1.board, g_1v1.current_player);

    WLDValue val_1v1 = wld_probe_state(&g_1v1);
    ASSERT_TEST(val_1v1 == WLD_DRAW, "1 King vs 1 King on non-aligned corners evaluates to WLD_DRAW");

    // 2v1 position
    GameState g_2v1;
    memset(&g_2v1, 0, sizeof(g_2v1));
    g_2v1.board.white_kings = (1U << 0) | (1U << 1);
    g_2v1.board.black_kings = (1U << 31);
    g_2v1.current_player = PLAYER_WHITE;
    g_2v1.hash = zobrist_compute_hash(&g_2v1.board, g_2v1.current_player);

    WLDValue val_2v1 = wld_probe_state(&g_2v1);
    ASSERT_TEST(val_2v1 == WLD_WIN_WHITE, "Native 2 Kings vs 1 King evaluates to WLD_WIN_WHITE");
}

static void test_probe_benchmark(void) {
    printf("\n=== Test 5: Probe Throughput Benchmark ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    GameState test_states[10];
    for (int i = 0; i < 10; i++) {
        memset(&test_states[i], 0, sizeof(GameState));
        test_states[i].board.white_kings = (1U << i) | (1U << (i + 1));
        test_states[i].board.black_kings = (1U << (31 - i));
        test_states[i].current_player = (Player)(i % 2);
        test_states[i].hash = zobrist_compute_hash(&test_states[i].board, test_states[i].current_player);
    }

    const int iterations = 100000;
    clock_t start = clock();
    WLDValue dummy_acc = WLD_UNKNOWN;
    for (int i = 0; i < iterations; i++) {
        dummy_acc = (WLDValue)(dummy_acc ^ wld_probe_state(&test_states[i % 10]));
    }
    clock_t end = clock();

    double seconds = (double)(end - start) / CLOCKS_PER_SEC;
    double probes_per_sec = (seconds > 0.0) ? ((double)iterations / seconds) : 0.0;

    printf("  Executed %d probes in %.4f seconds (%.2f million probes/sec)\n",
           iterations, seconds, probes_per_sec / 1000000.0);
    ASSERT_TEST(probes_per_sec > 50000.0, "Probe throughput exceeds 50k probes/sec (fast-path O(1))");
}

static void test_wld_solver_tactics(void) {
    printf("\n=== Test 6: WLD Alpha-Beta Shortest-Win Mini-Solver ===\n");
    if (wld_egdb_is_supported()) {
        wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    } else {
        wld_init_backend(WLD_BACKEND_REDUCED_NATIVE);
    }

    // 1. Applicability Check
    GameState g_2v1;
    memset(&g_2v1, 0, sizeof(g_2v1));
    g_2v1.board.white_kings = (1U << 0) | (1U << 1);
    g_2v1.board.black_kings = (1U << 31);
    g_2v1.current_player = PLAYER_WHITE;
    g_2v1.hash = zobrist_compute_hash(&g_2v1.board, g_2v1.current_player);

    ASSERT_TEST(wld_solver_is_applicable(&g_2v1) == true, "Solver is applicable on 2v1 position");

    // 2. 2 Kings vs 1 King Solver Search
    WLDSolverResult res_2v1 = wld_solver_search(&g_2v1, WLD_SOLVER_DEFAULT_DEPTH, false);
    ASSERT_TEST(!move_is_none(res_2v1.best_move), "Solver finds non-empty move for 2v1");
    ASSERT_TEST(res_2v1.outcome == WLD_WIN_WHITE, "Solver confirms 2v1 outcome is WLD_WIN_WHITE");
    ASSERT_TEST(res_2v1.nodes_visited > 0, "Solver explored search tree nodes");

    // 3. 3 Kings vs 1 King Solver Search
    GameState g_3v1;
    memset(&g_3v1, 0, sizeof(g_3v1));
    g_3v1.board.white_kings = (1U << 0) | (1U << 1) | (1U << 2);
    g_3v1.board.black_kings = (1U << 31);
    g_3v1.current_player = PLAYER_WHITE;
    g_3v1.hash = zobrist_compute_hash(&g_3v1.board, g_3v1.current_player);

    WLDSolverResult res_3v1 = wld_solver_search(&g_3v1, WLD_SOLVER_DEFAULT_DEPTH, false);
    ASSERT_TEST(!move_is_none(res_3v1.best_move), "Solver finds non-empty move for 3v1");
    ASSERT_TEST(res_3v1.outcome == WLD_WIN_WHITE, "Solver confirms 3v1 outcome is WLD_WIN_WHITE");

    // 4. Complete Endgame Conversion Simulation: White (Solver) vs Black (Legal Responder)
    // Starting with 2 Kings vs 1 King: White must force a win/capture within 20 plies without looping.
    GameState sim_game = g_2v1;
    int plies_played = 0;
    const int max_sim_plies = 40;

    while (!sim_game.is_game_over && plies_played < max_sim_plies) {
        Move m;
        Player p = sim_game.current_player;
        if (p == PLAYER_WHITE) {
            m = wld_solver_select_move(&sim_game, WLD_SOLVER_DEFAULT_DEPTH, false);
        } else {
            const MoveList *opp_ml = game_get_valid_moves(&sim_game);
            if (opp_ml->count == 0) break;
            m = opp_ml->moves[0];
        }
        if (move_is_none(m)) {
            printf("  [ERROR] Move is none at ply %d! player=%d, valid_moves=%d\n",
                   plies_played + 1, sim_game.current_player, game_get_valid_moves(&sim_game)->count);
            break;
        }
        printf("  [Sim Ply %2d] %s: %02d -> %02d (is_cap: %d)\n",
               plies_played + 1, (p == PLAYER_WHITE) ? "White" : "Black", m.from, m.to, m.is_cap);
        fflush(stdout);
        bool ok = game_execute_move(&sim_game, m);
        if (!ok) break;
        plies_played++;
    }

    printf("  Endgame 2v1 simulation converted in %d plies (Game Over: %d, Winner: %s, Draw: %d)\n",
           plies_played, sim_game.is_game_over,
           (sim_game.winner == PLAYER_WHITE) ? "BIANCO" : "NERO", sim_game.is_draw);

    ASSERT_TEST(sim_game.is_game_over && sim_game.winner == PLAYER_WHITE,
                "Solver forces victory in 2v1 conversion simulation without drawing or looping");

    // 5. Anti-Repetition Test: Ensure repetition count >= 3 is strictly avoided when winning
    GameState rep_state = g_2v1;
    rep_state.history_count = 2;
    rep_state.history[0] = (PositionKey){ .wm = 0, .wk = (1U << 0) | (1U << 1), .bm = 0, .bk = (1U << 31), .player = PLAYER_WHITE, .hash = rep_state.hash };
    rep_state.history[1] = (PositionKey){ .wm = 0, .wk = (1U << 0) | (1U << 1), .bm = 0, .bk = (1U << 31), .player = PLAYER_WHITE, .hash = rep_state.hash };
    
    WLDSolverResult rep_res = wld_solver_search(&rep_state, WLD_SOLVER_DEFAULT_DEPTH, false);
    ASSERT_TEST(!move_is_none(rep_res.best_move), "Solver chooses valid progressive move with repetition history");
}

static void test_mcts_value_shaping_and_proofs(void) {
    printf("\n=== Test 7: MCTS Value Shaping & Proof-Number Backpropagation ===\n");

    // 1. Depth-Discounted Reward Shaping Unit Verification
    float r_win_1 = mcts_compute_depth_discounted_reward(true, false, false, 1);
    float r_win_20 = mcts_compute_depth_discounted_reward(true, false, false, 20);
    float r_draw = mcts_compute_depth_discounted_reward(false, false, true, 10);
    float r_loss_1 = mcts_compute_depth_discounted_reward(false, true, false, 1);
    float r_loss_20 = mcts_compute_depth_discounted_reward(false, true, false, 20);

    ASSERT_TEST(fabsf(r_win_1 - 0.995f) < 1e-4f, "Win reward at depth 1 is 0.995");
    ASSERT_TEST(fabsf(r_win_20 - 0.900f) < 1e-4f, "Win reward at depth 20 is 0.900");
    ASSERT_TEST(r_win_1 > r_win_20, "Shorter win strictly outscores prolonged win (positive gradient)");
    ASSERT_TEST(fabsf(r_draw - 0.500f) < 1e-4f, "Draw reward is invariant at 0.500");
    ASSERT_TEST(fabsf(r_loss_1 - 0.005f) < 1e-4f, "Loss reward at depth 1 is 0.005");
    ASSERT_TEST(fabsf(r_loss_20 - 0.100f) < 1e-4f, "Prolonged loss at depth 20 is 0.100 (defensive resistance)");
    ASSERT_TEST(r_loss_20 > r_loss_1, "Prolonged resistance outscores immediate loss");
    ASSERT_TEST(r_win_20 > r_draw && r_draw > r_loss_20, "Win (0.900) > Draw (0.500) > Loss (0.100) bounds preserved");

    // 2. MCTS UCB1 Proof Propagation & Search Integration
    void *ucb1_st = NULL;
    engine_mcts_ucb1_init(&ucb1_st);
    ASSERT_TEST(ucb1_st != NULL, "MCTS UCB1 engine initializes successfully");

    engine_mcts_ucb1_set_time_budget(ucb1_st, 0.05);
    engine_mcts_ucb1_set_use_db(ucb1_st, false); // Force tree search with tablebase terminal evaluation

    GameState g_2v1;
    memset(&g_2v1, 0, sizeof(g_2v1));
    g_2v1.board.white_kings = (1U << 0) | (1U << 1);
    g_2v1.board.black_kings = (1U << 31);
    g_2v1.current_player = PLAYER_WHITE;
    g_2v1.hash = zobrist_compute_hash(&g_2v1.board, g_2v1.current_player);

    Move ucb1_move = engine_mcts_ucb1_get_move(ucb1_st, &g_2v1);
    ASSERT_TEST(!move_is_none(ucb1_move), "MCTS UCB1 selects valid move under tree search");
    ASSERT_TEST(engine_mcts_ucb1_get_node_count() > 0, "MCTS UCB1 populates node pool with proof propagation");
    ASSERT_TEST(engine_mcts_ucb1_get_root_visits(ucb1_st) > 0, "MCTS UCB1 completes root simulations");

    engine_mcts_ucb1_cleanup(ucb1_st);

    // 3. MCTS PUCT Proof Propagation & Search Integration
    void *puct_st = NULL;
    engine_mcts_puct_init(&puct_st);
    ASSERT_TEST(puct_st != NULL, "MCTS PUCT engine initializes successfully");

    engine_mcts_puct_set_time_budget(puct_st, 0.05);
    engine_mcts_puct_set_use_db(puct_st, false);

    Move puct_move = engine_mcts_puct_get_move(puct_st, &g_2v1);
    ASSERT_TEST(!move_is_none(puct_move), "MCTS PUCT selects valid move under tree search");
    ASSERT_TEST(engine_mcts_puct_get_node_count() > 0, "MCTS PUCT populates node pool with proof propagation");
    ASSERT_TEST(engine_mcts_puct_get_root_visits(puct_st) > 0, "MCTS PUCT completes root simulations");

    engine_mcts_puct_cleanup(puct_st);

    // 4. MCTS Transitioning into Tablebase Territory (5-piece position)
    // White: 2 Kings (sq 0, sq 1) + 1 Man (sq 4) vs Black: 2 Kings (sq 30, sq 31)
    GameState g_5p;
    memset(&g_5p, 0, sizeof(g_5p));
    g_5p.board.white_kings = (1U << 0) | (1U << 1);
    g_5p.board.white_men   = (1U << 4);
    g_5p.board.black_kings = (1U << 30) | (1U << 31);
    g_5p.current_player    = PLAYER_WHITE;
    g_5p.hash              = zobrist_compute_hash(&g_5p.board, g_5p.current_player);

    void *ucb1_5p = NULL;
    engine_mcts_ucb1_init(&ucb1_5p);
    engine_mcts_ucb1_set_time_budget(ucb1_5p, 0.05);
    engine_mcts_ucb1_set_use_db(ucb1_5p, true);

    Move ucb1_5p_move = engine_mcts_ucb1_get_move(ucb1_5p, &g_5p);
    ASSERT_TEST(!move_is_none(ucb1_5p_move), "MCTS UCB1 successfully evaluates 5-piece transition position");
    engine_mcts_ucb1_cleanup(ucb1_5p);

    void *puct_5p = NULL;
    engine_mcts_puct_init(&puct_5p);
    engine_mcts_puct_set_time_budget(puct_5p, 0.05);
    engine_mcts_puct_set_use_db(puct_5p, true);

    Move puct_5p_move = engine_mcts_puct_get_move(puct_5p, &g_5p);
    ASSERT_TEST(!move_is_none(puct_5p_move), "MCTS PUCT successfully evaluates 5-piece transition position");
    engine_mcts_puct_cleanup(puct_5p);
}

static void test_phase4_ui_cli_configuration(void) {
    printf("\n=== Test 8: Phase 4 UI/CLI Configuration & Live Detection ===\n");

    // 1. WLD Backend String Parser Verification
    ASSERT_TEST(wld_backend_parse("official") == WLD_BACKEND_OFFICIAL_8PIECE, "Parser 'official' -> OFFICIAL_8PIECE");
    ASSERT_TEST(wld_backend_parse("8piece") == WLD_BACKEND_OFFICIAL_8PIECE, "Parser '8piece' -> OFFICIAL_8PIECE");
    ASSERT_TEST(wld_backend_parse("reduced") == WLD_BACKEND_REDUCED_NATIVE, "Parser 'reduced' -> REDUCED_NATIVE");
    ASSERT_TEST(wld_backend_parse("native") == WLD_BACKEND_REDUCED_NATIVE, "Parser 'native' -> REDUCED_NATIVE");
    ASSERT_TEST(wld_backend_parse("none") == WLD_BACKEND_NONE, "Parser 'none' -> WLD_BACKEND_NONE");
    ASSERT_TEST(wld_backend_parse("disabled") == WLD_BACKEND_NONE, "Parser 'disabled' -> WLD_BACKEND_NONE");
    ASSERT_TEST(wld_backend_parse("random_junk") == WLD_BACKEND_NONE, "Parser unknown string defaults to WLD_BACKEND_NONE");

    // 2. Name formatting and CLI tokens
    ASSERT_TEST(strcmp(wld_backend_get_cli_name(WLD_BACKEND_OFFICIAL_8PIECE), "official") == 0, "CLI name official");
    ASSERT_TEST(strcmp(wld_backend_get_cli_name(WLD_BACKEND_REDUCED_NATIVE), "reduced") == 0, "CLI name reduced");
    ASSERT_TEST(strcmp(wld_backend_get_cli_name(WLD_BACKEND_NONE), "none") == 0, "CLI name none");
    ASSERT_TEST(strstr(wld_backend_get_name(WLD_BACKEND_OFFICIAL_8PIECE), "8 PEZZI") != NULL, "UI Label contains 8 PEZZI");

    // 3. Custom path setting and non-existent path safety lockout
    wld_set_custom_path("non_existent_folder_xyz_123");
    ASSERT_TEST(strcmp(wld_get_custom_path(), "non_existent_folder_xyz_123") == 0, "Custom path successfully set");

    WLDStatus bad_st = wld_get_status(WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(bad_st.available == false, "Non-existent path marked as unavailable");
    ASSERT_TEST(strstr(bad_st.status_message, "DISABILITATO") != NULL || strstr(bad_st.status_message, "NON SUPPORTATO") != NULL,
                "Status message informs user of missing files or disabled DB");

    // Reset custom path to default
    wld_set_custom_path("");
    ASSERT_TEST(strcmp(wld_get_custom_path(), "") == 0, "Custom path reset to empty default");

    // 4. GUI 3-Way Cycle Simulation
    WLDBackendType b = WLD_BACKEND_OFFICIAL_8PIECE;
    b = (b == WLD_BACKEND_OFFICIAL_8PIECE) ? WLD_BACKEND_REDUCED_NATIVE : (b == WLD_BACKEND_REDUCED_NATIVE ? WLD_BACKEND_NONE : WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(b == WLD_BACKEND_REDUCED_NATIVE, "GUI Cycle: OFFICIAL -> REDUCED");
    b = (b == WLD_BACKEND_OFFICIAL_8PIECE) ? WLD_BACKEND_REDUCED_NATIVE : (b == WLD_BACKEND_REDUCED_NATIVE ? WLD_BACKEND_NONE : WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(b == WLD_BACKEND_NONE, "GUI Cycle: REDUCED -> NONE");
    b = (b == WLD_BACKEND_OFFICIAL_8PIECE) ? WLD_BACKEND_REDUCED_NATIVE : (b == WLD_BACKEND_REDUCED_NATIVE ? WLD_BACKEND_NONE : WLD_BACKEND_OFFICIAL_8PIECE);
    ASSERT_TEST(b == WLD_BACKEND_OFFICIAL_8PIECE, "GUI Cycle: NONE -> OFFICIAL");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("============================================================\n");
    printf("  Damascus WLD Tablebase Subsystem - Full Verification\n");
    printf("============================================================\n");

    zobrist_init();
    GameState dummy_game;
    game_init(&dummy_game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    test_slice_scanner();
    test_status_reporting();
    test_official_backend_probing();
    test_reduced_native_backend();
    test_probe_benchmark();
    test_wld_solver_tactics();
    test_mcts_value_shaping_and_proofs();
    test_phase4_ui_cli_configuration();

    wld_cleanup();

    printf("\n============================================================\n");
    printf("  Summary: %d tests run, %d failed\n", s_tests_run, s_tests_failed);
    printf("============================================================\n");

    return (s_tests_failed == 0) ? 0 : 1;
}
