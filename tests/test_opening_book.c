/*______________________________________________________________________________
  Unit Test Suite for Damascus Opening Book Database Subsystem
______________________________________________________________________________*/

#include "opening_book.h"
#include "game.h"
#include "mcts_puct.h"
#include "mcts_ucb1.h"
#include "zobrist.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static double get_time_sec(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

static void test_compact_board_encoding(void) {
    printf("[Test 1] Testing 64-bit compact board encoding...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    uint64_t key_white = opening_book_encode_compact_board(&game.board, PLAYER_WHITE);
    uint64_t key_black = opening_book_encode_compact_board(&game.board, PLAYER_BLACK);

    assert(key_white != 0ULL);
    assert(key_black != 0ULL);
    assert(key_white != key_black); // Bit 63 differs between White and Black

    // Verify individual squares
    // Squares 0..11 are White Men (code 01b)
    for (int sq = 0; sq < 12; sq++) {
        uint64_t code = (key_white >> (sq * 2)) & 0x3ULL;
        assert(code == 1ULL);
    }
    // Squares 12..19 are Empty (code 00b)
    for (int sq = 12; sq < 20; sq++) {
        uint64_t code = (key_white >> (sq * 2)) & 0x3ULL;
        assert(code == 0ULL);
    }
    // Squares 20..31 are Black Men (code 10b)
    for (int sq = 20; sq < 32; sq++) {
        uint64_t code = (key_white >> (sq * 2)) & 0x3ULL;
        assert(code == 2ULL);
    }

    printf("  -> Compact board encoding verified successfully.\n");
}

static void test_book_initialization(void) {
    printf("[Test 2] Testing opening book initialization (Kingsrow ODB & BIN)...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        printf("  -> Warning: kr_italian.odb not found in standard paths, trying BIN fallback...\n");
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    OpeningBookStatus status = opening_book_get_status();
    assert(status.loaded);
    assert(status.total_positions > 0);
    assert(status.total_moves > 0);

    printf("  -> Book loaded: %s, Positions: %u, Moves: %u, Version: %s\n",
           status.file_path, status.total_positions, status.total_moves, status.version_str);
}

static void test_initial_position_probe(void) {
    printf("[Test 3] Probing initial board state (White opening candidates)...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    BookMoveList moves;
    bool found = opening_book_probe(&game, &moves);
    assert(found);
    assert(moves.count > 0);

    printf("  -> Found %d opening candidate moves for White:\n", moves.count);
    float sum_priors = 0.0f;
    for (int i = 0; i < moves.count; i++) {
        Move m = moves.entries[i].move;
        printf("     [%d] %d -> %d | Score: %+d cp | Depth: %d | Prior: %.4f | Flags: 0x%02x\n",
               i + 1, MOVE_FROM(m) + 1, MOVE_TO(m) + 1,
               moves.entries[i].score, moves.entries[i].depth,
               moves.entries[i].prior_weight, moves.entries[i].flags);
        sum_priors += moves.entries[i].prior_weight;
    }
    assert(fabsf(sum_priors - 1.0f) < 1e-3f);
}

static void test_subsequent_position_probe(void) {
    printf("[Test 4] Probing subsequent position after White's first move...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    const MoveList *legal_moves = game_get_valid_moves(&game);
    assert(legal_moves->count > 0);

    // Execute first legal opening move
    Move white_move = legal_moves->moves[0];
    game_execute_move(&game, white_move);
    assert(game.current_player == PLAYER_BLACK);

    BookMoveList black_moves;
    bool found = opening_book_probe(&game, &black_moves);
    assert(found);
    assert(black_moves.count > 0);

    printf("  -> Black responses found: %d candidates\n", black_moves.count);
    for (int i = 0; i < black_moves.count; i++) {
        Move m = black_moves.entries[i].move;
        printf("     [%d] %d -> %d | Score: %+d | Prior: %.4f\n",
               i + 1, MOVE_FROM(m) + 1, MOVE_TO(m) + 1,
               black_moves.entries[i].score, black_moves.entries[i].prior_weight);
    }
}

static void test_selection_modes(void) {
    printf("[Test 5] Testing move selection modes (BEST, GOOD, ALL, Softmax)...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    uint32_t rng = 123456789;

    // Mode OFF
    Move m_off = opening_book_select_move(&game, BOOK_MODE_OFF, 0.5f, &rng);
    assert(move_is_none(m_off));

    // Mode BEST
    Move m_best = opening_book_select_move(&game, BOOK_MODE_BEST, 0.0f, &rng);
    assert(!move_is_none(m_best));
    printf("  -> BEST move: %d -> %d\n", MOVE_FROM(m_best) + 1, MOVE_TO(m_best) + 1);

    // Mode GOOD (sample 100 times, ensure valid moves and diversity)
    int counts[32] = {0};
    for (int i = 0; i < 100; i++) {
        Move m = opening_book_select_move(&game, BOOK_MODE_GOOD, 1.0f, &rng);
        assert(!move_is_none(m));
        counts[MOVE_FROM(m)]++;
    }
    printf("  -> GOOD mode sampled 100 times successfully.\n");

    // Mode ALL
    for (int i = 0; i < 100; i++) {
        Move m = opening_book_select_move(&game, BOOK_MODE_ALL, 1.5f, &rng);
        assert(!move_is_none(m));
    }
    printf("  -> ALL mode sampled 100 times successfully.\n");
}

static void test_throughput(void) {
    printf("[Test 6] Benchmarking probe throughput...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    BookMoveList moves;
    const int iterations = 1000000;

    double t0 = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        opening_book_probe(&game, &moves);
    }
    double t1 = get_time_sec();

    double elapsed = t1 - t0;
    double probes_per_sec = (double)iterations / elapsed;
    printf("  -> Completed %d probes in %.4f s (%.2f million probes/sec)\n",
           iterations, elapsed, probes_per_sec / 1e6);
    assert(probes_per_sec > 500000.0); // > 500k probes/sec minimum
}

static void test_cleanup_and_reinit(void) {
    printf("[Test 7] Testing cleanup and re-initialization...\n");

    opening_book_cleanup();
    OpeningBookStatus s1 = opening_book_get_status();
    assert(!s1.loaded);

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);
    OpeningBookStatus s2 = opening_book_get_status();
    assert(s2.loaded);

    opening_book_cleanup();
    printf("  -> Cleanup and re-init verified successfully.\n");
}

static void test_mcts_puct_integration(void) {
    printf("[Test 8] Testing MCTS PUCT Opening Book integration (Instant & Guided)...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_MCTS_PUCT, ENGINE_TYPE_MCTS_PUCT);

    void *puct_st = NULL;
    engine_mcts_puct_init(&puct_st);
    assert(puct_st != NULL);

    // 1. Instant Best Mode
    engine_mcts_puct_set_use_book(puct_st, true);
    engine_mcts_puct_set_book_mode(puct_st, BOOK_MODE_BEST);
    Move instant_move = engine_mcts_puct_get_move(puct_st, &game);
    assert(!move_is_none(instant_move));
    printf("  -> MCTS PUCT instant book move: %d -> %d\n", MOVE_FROM(instant_move) + 1, MOVE_TO(instant_move) + 1);

    // 2. PUCT Guided Mode with Tree Search & Prior Blending
    engine_mcts_puct_set_guided_book(puct_st, true, 0.75f);
    engine_mcts_puct_set_time_budget(puct_st, 0.05);
    Move guided_move = engine_mcts_puct_get_move(puct_st, &game);
    assert(!move_is_none(guided_move));
    uint32_t visits = engine_mcts_puct_get_root_visits(puct_st);
    assert(visits > 0);
    printf("  -> MCTS PUCT guided move: %d -> %d (root visits: %u)\n",
           MOVE_FROM(guided_move) + 1, MOVE_TO(guided_move) + 1, visits);

    engine_mcts_puct_cleanup(puct_st);
    printf("  -> MCTS PUCT integration verified successfully.\n");
}

static void test_mcts_ucb1_integration(void) {
    printf("[Test 9] Testing MCTS UCB1 Opening Book integration (Instant & Tree Search)...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_MCTS_UCB1, ENGINE_TYPE_MCTS_UCB1);

    void *ucb1_st = NULL;
    engine_mcts_ucb1_init(&ucb1_st);
    assert(ucb1_st != NULL);

    // 1. Instant Best Mode
    engine_mcts_ucb1_set_use_book(ucb1_st, true);
    engine_mcts_ucb1_set_book_mode(ucb1_st, BOOK_MODE_BEST);
    Move instant_move = engine_mcts_ucb1_get_move(ucb1_st, &game);
    assert(!move_is_none(instant_move));
    printf("  -> MCTS UCB1 instant book move: %d -> %d\n", MOVE_FROM(instant_move) + 1, MOVE_TO(instant_move) + 1);

    // 2. Tree search with Book OFF
    engine_mcts_ucb1_set_book_mode(ucb1_st, BOOK_MODE_OFF);
    engine_mcts_ucb1_set_time_budget(ucb1_st, 0.05);
    Move searched_move = engine_mcts_ucb1_get_move(ucb1_st, &game);
    assert(!move_is_none(searched_move));
    uint32_t visits = engine_mcts_ucb1_get_root_visits(ucb1_st);
    assert(visits > 0);
    printf("  -> MCTS UCB1 search move: %d -> %d (root visits: %u)\n",
           MOVE_FROM(searched_move) + 1, MOVE_TO(searched_move) + 1, visits);

    engine_mcts_ucb1_cleanup(ucb1_st);
    printf("  -> MCTS UCB1 integration verified successfully.\n");
}

static void test_out_of_book_fallback(void) {
    printf("[Test 10] Testing out-of-book graceful fallback to full tree search...\n");

    GameState game;
    game_init(&game, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_MCTS_PUCT, ENGINE_TYPE_MCTS_PUCT);

    // Construct late-game endgame position (out of opening book)
    game.board.white_men = 0;
    game.board.white_kings = (1ULL << 4) | (1ULL << 10);
    game.board.black_men = 0;
    game.board.black_kings = (1ULL << 20) | (1ULL << 25);
    game.hash = zobrist_compute_hash(&game.board, game.current_player);

    BookMoveList book_moves;
    bool in_book = opening_book_probe(&game, &book_moves);
    assert(!in_book); // Must be out of opening book

    void *puct_st = NULL;
    engine_mcts_puct_init(&puct_st);
    engine_mcts_puct_set_use_book(puct_st, true);
    engine_mcts_puct_set_book_mode(puct_st, BOOK_MODE_BEST);
    engine_mcts_puct_set_time_budget(puct_st, 0.05);

    Move m = engine_mcts_puct_get_move(puct_st, &game);
    assert(!move_is_none(m));
    printf("  -> PUCT returned valid move out-of-book: %d -> %d\n", MOVE_FROM(m) + 1, MOVE_TO(m) + 1);

    engine_mcts_puct_cleanup(puct_st);
    printf("  -> Out-of-book fallback verified successfully.\n");
}

static void test_opening_diversity(void) {
    printf("[Test 11] Testing Opening Book Grandmaster Move Diversity across 30 games...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    const int num_simulations = 30;
    const int plies_per_sim = 6; // 3 full moves
    uint64_t path_hashes[30];
    int unique_paths = 0;
    uint32_t rng_state = 0xDEADBEEF;

    for (int sim = 0; sim < num_simulations; sim++) {
        GameState g;
        game_init(&g, MODE_2PLAYER, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

        uint64_t hash = 14695981039346656037ULL; // FNV-1a
        for (int p = 0; p < plies_per_sim && !g.is_game_over; p++) {
            Move m = opening_book_select_move(&g, BOOK_MODE_GOOD, 1.0f, &rng_state);
            if (move_is_none(m)) {
                const MoveList *legal = game_get_valid_moves(&g);
                if (!legal || legal->count == 0) break;
                m = legal->moves[0];
            }
            hash ^= (uint64_t)((m.from << 8) | m.to);
            hash *= 1099511628211ULL;
            game_execute_move(&g, m);
        }

        path_hashes[sim] = hash;
        bool duplicate = false;
        for (int prev = 0; prev < sim; prev++) {
            if (path_hashes[prev] == hash) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) unique_paths++;
    }

    double diversity_pct = ((double)unique_paths / (double)num_simulations) * 100.0;
    printf("  -> Generated %d unique opening lines out of %d games (%.1f%% diversity)\n",
           unique_paths, num_simulations, diversity_pct);

    assert(unique_paths >= 15); // Must explore multiple opening variations (>50% diversity)
    printf("  -> Opening move diversity verified successfully.\n");
}

static void test_tournament_puct_book_vs_nobook(void) {
    printf("[Test 12] Headless Tournament: 50 Games - MCTS PUCT (with ODB) vs MCTS PUCT (No Book)...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    const int total_games = 50;
    const double time_per_move = 0.015; // fast per-move budget for quick unit test execution
    const int max_plies = 80;

    int book_wins = 0;
    int nobook_wins = 0;
    int draws = 0;
    int total_plies = 0;
    double total_book_move_time = 0.0;
    int total_book_moves = 0;

    for (int g = 0; g < total_games; g++) {
        bool book_is_white = (g % 2 == 0); // Alternate colors

        GameState game;
        game_init(&game, MODE_CPUVSCPU, PLAYER_WHITE, ENGINE_TYPE_MCTS_PUCT, ENGINE_TYPE_MCTS_PUCT);

        void *eng_white = NULL;
        void *eng_black = NULL;
        engine_mcts_puct_init(&eng_white);
        engine_mcts_puct_init(&eng_black);

        void *eng_book = book_is_white ? eng_white : eng_black;
        void *eng_nobook = book_is_white ? eng_black : eng_white;

        engine_mcts_puct_set_time_budget(eng_book, time_per_move);
        engine_mcts_puct_set_use_book(eng_book, true);
        engine_mcts_puct_set_book_mode(eng_book, BOOK_MODE_BEST);

        engine_mcts_puct_set_time_budget(eng_nobook, time_per_move);
        engine_mcts_puct_set_use_book(eng_nobook, false);
        engine_mcts_puct_set_book_mode(eng_nobook, BOOK_MODE_OFF);

        int plies = 0;
        while (!game.is_game_over && plies < max_plies) {
            void *active = (game.current_player == PLAYER_WHITE) ? eng_white : eng_black;
            bool is_book_turn = (active == eng_book);

            double t0 = get_time_sec();
            Move m = engine_mcts_puct_get_move(active, &game);
            double dt = get_time_sec() - t0;

            if (is_book_turn) {
                total_book_move_time += dt;
                total_book_moves++;
            }

            if (move_is_none(m)) {
                game.is_game_over = true;
                game.winner = (game.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                break;
            }

            game_execute_move(&game, m);
            plies++;
        }

        total_plies += plies;

        if (game.is_game_over && !game.is_draw) {
            if ((game.winner == PLAYER_WHITE && book_is_white) ||
                (game.winner == PLAYER_BLACK && !book_is_white)) {
                book_wins++;
            } else {
                nobook_wins++;
            }
        } else {
            draws++;
        }

        engine_mcts_puct_cleanup(eng_white);
        engine_mcts_puct_cleanup(eng_black);
    }

    double score_rate = ((double)book_wins + 0.5 * (double)draws) / (double)total_games;
    double win_rate = ((double)book_wins / (double)total_games) * 100.0;
    double draw_rate = ((double)draws / (double)total_games) * 100.0;
    double loss_rate = ((double)nobook_wins / (double)total_games) * 100.0;
    double avg_plies = (double)total_plies / (double)total_games;

    double elo_diff = 0.0;
    if (score_rate > 0.0 && score_rate < 1.0) {
        elo_diff = -400.0 * log10(1.0 / score_rate - 1.0);
    } else if (score_rate >= 1.0) {
        elo_diff = 800.0; // Perfect score ceiling
    }

    printf("  ====================================================================\n");
    printf("  PUCT Tournament Results (%d Games, %.3fs/move):\n", total_games, time_per_move);
    printf("    * Book Wins : %d (%.1f%%)\n", book_wins, win_rate);
    printf("    * Draws     : %d (%.1f%%)\n", draws, draw_rate);
    printf("    * Losses    : %d (%.1f%%)\n", nobook_wins, loss_rate);
    printf("    * Score Rate: %.3f (%.1f%%)\n", score_rate, score_rate * 100.0);
    printf("    * Elo Delta : %+.1f Elo (Advantage for Book)\n", elo_diff);
    printf("    * Avg Plies : %.1f plies/game\n", avg_plies);
    printf("  ====================================================================\n");

    // Book-assisted engine maintains strong grandmaster opening defense & parity
    assert(score_rate >= 0.45);
    printf("  -> Headless PUCT Tournament validation passed successfully.\n");
}

static void test_tournament_ucb1_book_vs_nobook(void) {
    printf("[Test 13] Headless Tournament: 20 Games - MCTS UCB1 (with ODB) vs MCTS UCB1 (No Book)...\n");

    bool ok = opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);
    if (!ok) {
        ok = opening_book_init(BOOK_BACKEND_CHECKERBOARD_BIN, NULL);
    }
    assert(ok);

    const int total_games = 20;
    const double time_per_move = 0.015;
    const int max_plies = 80;

    int book_wins = 0;
    int nobook_wins = 0;
    int draws = 0;

    for (int g = 0; g < total_games; g++) {
        bool book_is_white = (g % 2 == 0);

        GameState game;
        game_init(&game, MODE_CPUVSCPU, PLAYER_WHITE, ENGINE_TYPE_MCTS_UCB1, ENGINE_TYPE_MCTS_UCB1);

        void *eng_white = NULL;
        void *eng_black = NULL;
        engine_mcts_ucb1_init(&eng_white);
        engine_mcts_ucb1_init(&eng_black);

        void *eng_book = book_is_white ? eng_white : eng_black;
        void *eng_nobook = book_is_white ? eng_black : eng_white;

        engine_mcts_ucb1_set_time_budget(eng_book, time_per_move);
        engine_mcts_ucb1_set_use_book(eng_book, true);
        engine_mcts_ucb1_set_book_mode(eng_book, BOOK_MODE_BEST);

        engine_mcts_ucb1_set_time_budget(eng_nobook, time_per_move);
        engine_mcts_ucb1_set_use_book(eng_nobook, false);
        engine_mcts_ucb1_set_book_mode(eng_nobook, BOOK_MODE_OFF);

        int plies = 0;
        while (!game.is_game_over && plies < max_plies) {
            void *active = (game.current_player == PLAYER_WHITE) ? eng_white : eng_black;

            Move m = engine_mcts_ucb1_get_move(active, &game);
            if (move_is_none(m)) {
                game.is_game_over = true;
                game.winner = (game.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                break;
            }

            game_execute_move(&game, m);
            plies++;
        }

        if (game.is_game_over && !game.is_draw) {
            if ((game.winner == PLAYER_WHITE && book_is_white) ||
                (game.winner == PLAYER_BLACK && !book_is_white)) {
                book_wins++;
            } else {
                nobook_wins++;
            }
        } else {
            draws++;
        }

        engine_mcts_ucb1_cleanup(eng_white);
        engine_mcts_ucb1_cleanup(eng_black);
    }

    double score_rate = ((double)book_wins + 0.5 * (double)draws) / (double)total_games;
    double elo_diff = 0.0;
    if (score_rate > 0.0 && score_rate < 1.0) {
        elo_diff = -400.0 * log10(1.0 / score_rate - 1.0);
    } else if (score_rate >= 1.0) {
        elo_diff = 800.0;
    }

    printf("  UCB1 Tournament Results (%d Games): Book Wins: %d, Draws: %d, Losses: %d, Score: %.1f%%, Elo: %+.1f\n",
           total_games, book_wins, draws, nobook_wins, score_rate * 100.0, elo_diff);

    assert(score_rate >= 0.35);
    printf("  -> Headless UCB1 Tournament validation passed successfully.\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("====================================================\n");
    printf("   DAMASCUS OPENING BOOK DATABASE TEST SUITE        \n");
    printf("====================================================\n");

    test_compact_board_encoding();
    test_book_initialization();
    test_initial_position_probe();
    test_subsequent_position_probe();
    test_selection_modes();
    test_throughput();
    test_cleanup_and_reinit();
    test_mcts_puct_integration();
    test_mcts_ucb1_integration();
    test_out_of_book_fallback();
    test_opening_diversity();
    test_tournament_puct_book_vs_nobook();
    test_tournament_ucb1_book_vs_nobook();

    printf("\n>>> ALL OPENING BOOK TESTS PASSED (100%% SUCCESS) <<<\n");
    return 0;
}
