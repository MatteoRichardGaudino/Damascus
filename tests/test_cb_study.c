#include "game.h"
#include "engine_checkerboard.h"
#include "engine_mcts_puct.h"
#include "engine_mcts_ucb1.h"
#include "opening_book.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

typedef struct {
    const char *series_name;
    EngineType engine_a_type;
    bool a_use_book;
    BookPlayMode a_book_mode;
    EngineType engine_b_type;
    int total_games;
    double time_per_move;
} MatchExperiment;

typedef struct {
    int a_wins;
    int b_wins;
    int draws;
    int draws_3fold;
    int draws_repetition_while_cb_winning;
    int draws_repetition_while_a_winning;
    int draws_repetition_even;
    int total_plies;
    double total_time;
} MatchStats;

static void format_move_str(Move m, char *buf, size_t sz) {
    if (move_is_none(m)) {
        snprintf(buf, sz, "none");
        return;
    }
    // Convert to 1-based square notation (1..32)
    int from = MOVE_FROM(m) + 1;
    int to = MOVE_TO(m) + 1;
    if (MOVE_IS_CAP(m)) {
        snprintf(buf, sz, "%02dx%02d", from, to);
    } else {
        snprintf(buf, sz, "%02d-%02d", from, to);
    }
}

static void run_experiment(const MatchExperiment *exp, MatchStats *out_stats) {
    memset(out_stats, 0, sizeof(MatchStats));
    printf("\n======================================================================\n");
    printf("  RUNNING EXPERIMENT: %s\n", exp->series_name);
    printf("  Games: %d | Time/Move: %.2fs\n", exp->total_games, exp->time_per_move);
    printf("======================================================================\n");

    const int max_plies = 150;

    for (int g = 0; g < exp->total_games; g++) {
        bool a_is_white = (g % 2 == 0); // Alternate colors
        Player a_player = a_is_white ? PLAYER_WHITE : PLAYER_BLACK;
        Player b_player = a_is_white ? PLAYER_BLACK : PLAYER_WHITE;

        GameState game;
        game_init(&game, MODE_CPUVSCPU, PLAYER_WHITE,
                  a_is_white ? exp->engine_a_type : exp->engine_b_type,
                  a_is_white ? exp->engine_b_type : exp->engine_a_type);

        void *eng_a = NULL;
        void *eng_b = NULL;

        if (exp->engine_a_type == ENGINE_TYPE_MCTS_PUCT) {
            engine_mcts_puct_init(&eng_a);
            engine_mcts_puct_set_time_budget(eng_a, exp->time_per_move);
            engine_mcts_puct_set_use_book(eng_a, exp->a_use_book);
            engine_mcts_puct_set_book_mode(eng_a, exp->a_book_mode);
        } else if (exp->engine_a_type == ENGINE_TYPE_MCTS_UCB1) {
            engine_mcts_ucb1_init(&eng_a);
            engine_mcts_ucb1_set_time_budget(eng_a, exp->time_per_move);
            engine_mcts_ucb1_set_use_book(eng_a, exp->a_use_book);
            engine_mcts_ucb1_set_book_mode(eng_a, exp->a_book_mode);
        }

        if (exp->engine_b_type == ENGINE_TYPE_CHECKERBOARD) {
            engine_checkerboard_init(&eng_b);
            engine_checkerboard_set_search_time(eng_b, exp->time_per_move);
        }

        char opening_str[128] = {0};
        int plies = 0;
        double t_start = get_time_sec();

        while (!game.is_game_over && plies < max_plies) {
            Move m;
            bool is_a_turn = (game.current_player == a_player);
            if (is_a_turn) {
                if (exp->engine_a_type == ENGINE_TYPE_MCTS_PUCT) {
                    m = engine_mcts_puct_get_move(eng_a, &game);
                } else {
                    m = engine_mcts_ucb1_get_move(eng_a, &game);
                }
            } else {
                m = engine_checkerboard_get_move(eng_b, &game);
            }

            if (move_is_none(m)) {
                game.is_game_over = true;
                game.winner = (game.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                break;
            }

            // Record first 6 plies for opening study
            if (plies < 6) {
                char m_buf[16];
                format_move_str(m, m_buf, sizeof(m_buf));
                if (plies % 2 == 0) {
                    char temp[32];
                    snprintf(temp, sizeof(temp), "%d. %s ", (plies / 2) + 1, m_buf);
                    strcat(opening_str, temp);
                } else {
                    strcat(opening_str, m_buf);
                    strcat(opening_str, " ");
                }
            }

            game_execute_move(&game, m);
            plies++;
        }

        double duration = get_time_sec() - t_start;
        out_stats->total_plies += plies;
        out_stats->total_time += duration;

        // Piece counts at game end
        int wm = __builtin_popcount(game.board.white_men);
        int wk = __builtin_popcount(game.board.white_kings);
        int bm = __builtin_popcount(game.board.black_men);
        int bk = __builtin_popcount(game.board.black_kings);

        int a_total_pcs = a_is_white ? (wm + wk) : (bm + bk);
        int b_total_pcs = a_is_white ? (bm + bk) : (wm + wk);

        const char *res_str = "Draw";
        if (game.is_game_over && !game.is_draw) {
            if (game.winner == a_player) {
                out_stats->a_wins++;
                res_str = a_is_white ? "Engine A (White) WINS" : "Engine A (Black) WINS";
            } else {
                out_stats->b_wins++;
                res_str = a_is_white ? "CheckerBoard (Black) WINS" : "CheckerBoard (White) WINS";
            }
        } else {
            out_stats->draws++;
            if (game.is_draw) {
                out_stats->draws_3fold++;
                if (b_total_pcs > a_total_pcs) {
                    out_stats->draws_repetition_while_cb_winning++;
                    res_str = "Draw (3-Fold Repetition - CB was UP pieces!)";
                } else if (a_total_pcs > b_total_pcs) {
                    out_stats->draws_repetition_while_a_winning++;
                    res_str = "Draw (3-Fold Repetition - Engine A was UP pieces!)";
                } else {
                    out_stats->draws_repetition_even++;
                    res_str = "Draw (3-Fold Repetition - Material Even)";
                }
            } else {
                res_str = "Draw (Max Plies)";
            }
        }

        printf("  Game #%02d | %s vs %s | Plies: %3d | Pieces: A(%d) vs CB(%d) | %s | %.2fs\n",
               g + 1,
               a_is_white ? "A(W)" : "CB(W)",
               a_is_white ? "CB(B)" : "A(B)",
               plies, a_total_pcs, b_total_pcs, res_str, duration);
        printf("    -> Opening: %s\n", opening_str);

        if (exp->engine_a_type == ENGINE_TYPE_MCTS_PUCT) {
            engine_mcts_puct_cleanup(eng_a);
        } else if (exp->engine_a_type == ENGINE_TYPE_MCTS_UCB1) {
            engine_mcts_ucb1_cleanup(eng_a);
        }
        if (exp->engine_b_type == ENGINE_TYPE_CHECKERBOARD) {
            engine_checkerboard_cleanup(eng_b);
        }
    }

    double score_rate = ((double)out_stats->a_wins + 0.5 * (double)out_stats->draws) / (double)exp->total_games;
    printf("\n  EXPERIMENT SUMMARY [%s]:\n", exp->series_name);
    printf("    * Engine A Wins : %d (%.1f%%)\n", out_stats->a_wins, (double)out_stats->a_wins * 100.0 / exp->total_games);
    printf("    * CheckerBoard W: %d (%.1f%%)\n", out_stats->b_wins, (double)out_stats->b_wins * 100.0 / exp->total_games);
    printf("    * Draws         : %d (%.1f%%)\n", out_stats->draws, (double)out_stats->draws * 100.0 / exp->total_games);
    printf("    * 3-Fold Draws  : %d (CB was winning in %d of them)\n",
           out_stats->draws_3fold, out_stats->draws_repetition_while_cb_winning);
    printf("    * Score Rate (A): %.3f (%.1f%%)\n", score_rate, score_rate * 100.0);
    printf("    * Avg Plies/Game: %.1f\n", (double)out_stats->total_plies / exp->total_games);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("======================================================================\n");
    printf("   DAMASCUS STUDY: MCTS PUCT/UCB1 OPENING BOOK VS CHECKERBOARD        \n");
    printf("======================================================================\n");

    opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);

    MatchExperiment exp1 = {
        .series_name = "1. MCTS PUCT with Opening Book (BEST Mode) vs CheckerBoard",
        .engine_a_type = ENGINE_TYPE_MCTS_PUCT,
        .a_use_book = true,
        .a_book_mode = BOOK_MODE_BEST,
        .engine_b_type = ENGINE_TYPE_CHECKERBOARD,
        .total_games = 12,
        .time_per_move = 0.08
    };

    MatchExperiment exp2 = {
        .series_name = "2. MCTS PUCT with Opening Book (PUCT_GUIDED Mode) vs CheckerBoard",
        .engine_a_type = ENGINE_TYPE_MCTS_PUCT,
        .a_use_book = true,
        .a_book_mode = BOOK_MODE_PUCT_GUIDED,
        .engine_b_type = ENGINE_TYPE_CHECKERBOARD,
        .total_games = 12,
        .time_per_move = 0.08
    };

    MatchExperiment exp3 = {
        .series_name = "3. MCTS PUCT NO BOOK (Search Only) vs CheckerBoard",
        .engine_a_type = ENGINE_TYPE_MCTS_PUCT,
        .a_use_book = false,
        .a_book_mode = BOOK_MODE_OFF,
        .engine_b_type = ENGINE_TYPE_CHECKERBOARD,
        .total_games = 12,
        .time_per_move = 0.08
    };

    MatchExperiment exp4 = {
        .series_name = "4. MCTS UCB1 with Opening Book (BEST Mode) vs CheckerBoard",
        .engine_a_type = ENGINE_TYPE_MCTS_UCB1,
        .a_use_book = true,
        .a_book_mode = BOOK_MODE_BEST,
        .engine_b_type = ENGINE_TYPE_CHECKERBOARD,
        .total_games = 12,
        .time_per_move = 0.08
    };

    MatchExperiment exp5 = {
        .series_name = "5. MCTS UCB1 NO BOOK (Search Only) vs CheckerBoard",
        .engine_a_type = ENGINE_TYPE_MCTS_UCB1,
        .a_use_book = false,
        .a_book_mode = BOOK_MODE_OFF,
        .engine_b_type = ENGINE_TYPE_CHECKERBOARD,
        .total_games = 12,
        .time_per_move = 0.08
    };

    MatchStats s1, s2, s3, s4, s5;
    run_experiment(&exp1, &s1);
    run_experiment(&exp2, &s2);
    run_experiment(&exp3, &s3);
    run_experiment(&exp4, &s4);
    run_experiment(&exp5, &s5);

    printf("\n======================================================================\n");
    printf("   FINAL COMPARATIVE TABLE ACROSS ALL EXPERIMENTAL SERIES            \n");
    printf("======================================================================\n");
    printf(" Series                                  | Wins | Draws | Loss | Score%% | 3-Fold (CB Ahead)\n");
    printf("-----------------------------------------+------+-------+------+--------+-------------------\n");
    printf(" 1. PUCT (Book: BEST) vs CheckerBoard    | %4d | %5d | %4d | %5.1f%% | %6d (%d)\n",
           s1.a_wins, s1.draws, s1.b_wins, ((double)s1.a_wins + 0.5 * s1.draws) * 100.0 / 12.0, s1.draws_3fold, s1.draws_repetition_while_cb_winning);
    printf(" 2. PUCT (Book: GUIDED) vs CheckerBoard  | %4d | %5d | %4d | %5.1f%% | %6d (%d)\n",
           s2.a_wins, s2.draws, s2.b_wins, ((double)s2.a_wins + 0.5 * s2.draws) * 100.0 / 12.0, s2.draws_3fold, s2.draws_repetition_while_cb_winning);
    printf(" 3. PUCT (NO BOOK) vs CheckerBoard       | %4d | %5d | %4d | %5.1f%% | %6d (%d)\n",
           s3.a_wins, s3.draws, s3.b_wins, ((double)s3.a_wins + 0.5 * s3.draws) * 100.0 / 12.0, s3.draws_3fold, s3.draws_repetition_while_cb_winning);
    printf(" 4. UCB1 (Book: BEST) vs CheckerBoard    | %4d | %5d | %4d | %5.1f%% | %6d (%d)\n",
           s4.a_wins, s4.draws, s4.b_wins, ((double)s4.a_wins + 0.5 * s4.draws) * 100.0 / 12.0, s4.draws_3fold, s4.draws_repetition_while_cb_winning);
    printf(" 5. UCB1 (NO BOOK) vs CheckerBoard       | %4d | %5d | %4d | %5.1f%% | %6d (%d)\n",
           s5.a_wins, s5.draws, s5.b_wins, ((double)s5.a_wins + 0.5 * s5.draws) * 100.0 / 12.0, s5.draws_3fold, s5.draws_repetition_while_cb_winning);
    printf("======================================================================\n");

    opening_book_cleanup();
    return 0;
}
