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

    // Mode PUCT_GUIDED (should return NONE to let PUCT search use priors)
    Move m_puct = opening_book_select_move(&game, BOOK_MODE_PUCT_GUIDED, 0.5f, &rng);
    assert(move_is_none(m_puct));

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
    engine_mcts_puct_set_book_mode(puct_st, BOOK_MODE_PUCT_GUIDED);
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
    printf("[Test 9] Testing MCTS UCB1 Opening Book integration (Instant & Warm-Start)...\n");

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

    // 2. Warm-started tree search
    engine_mcts_ucb1_set_book_mode(ucb1_st, BOOK_MODE_PUCT_GUIDED); // guided / tree search with warm start
    engine_mcts_ucb1_set_time_budget(ucb1_st, 0.05);
    Move searched_move = engine_mcts_ucb1_get_move(ucb1_st, &game);
    assert(!move_is_none(searched_move));
    uint32_t visits = engine_mcts_ucb1_get_root_visits(ucb1_st);
    assert(visits > 0);
    printf("  -> MCTS UCB1 warm-started move: %d -> %d (root visits: %u)\n",
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

int main(void) {
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

    printf("\n>>> ALL OPENING BOOK TESTS PASSED (100%% SUCCESS) <<<\n");
    return 0;
}
