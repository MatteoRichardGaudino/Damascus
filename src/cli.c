#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include "game.h"
#include "engine.h"
#include "mcts_ucb1.h"
#include "mcts_puct.h"
#include "mcts_heuristic.h"
#include "wld_db.h"
#include "zobrist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#include <errno.h>

#define CLI_MAX_GAMES 10000
#define DEFAULT_MAX_PLIES 250
#define DEFAULT_OPENING_PLIES 2

static int detect_cpu_cores(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
    int count = 1;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0) == 0 && count >= 1) {
        return count;
    }
    return 4;
#elif defined(_SC_NPROCESSORS_ONLN)
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1) nprocs = 1;
    return (int)nprocs;
#else
    return 4;
#endif
}

typedef struct {
    int        game_index;
    EngineType white_engine;
    EngineType black_engine;
    bool       is_draw;
    Player     winner;
    char       reason[64];
    int        plies;
    double     white_total_time;
    int        white_move_count;
    double     black_total_time;
    int        black_move_count;
    double     total_duration;
    int        white_pieces_remaining;
    int        black_pieces_remaining;
} GameRecord;

typedef struct {
    EngineType type;
    char       name[32];
    int        games_played;
    int        wins;
    int        draws;
    int        losses;
    double     points;      // win = 1.0, draw = 0.5, loss = 0.0
    double     score_pct;   // points / games_played * 100
    int        total_plies;
    double     total_time;
    int        total_moves;
} TournamentEngineStats;

static inline double cli_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void ensure_parent_dir_exists(const char *filepath) {
    if (!filepath || !*filepath) return;
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s", filepath);
    char *last_slash = strrchr(path_buf, '/');
    if (last_slash) {
        *last_slash = '\0';
        #ifdef _WIN32
        mkdir(path_buf);
        #else
        mkdir(path_buf, 0755);
        #endif
    }
}

static EngineType parse_engine_name(const char *name) {
    if (!name) return ENGINE_TYPE_RANDOM;
    if (strcasecmp(name, "mcts_ucb1") == 0 || strcasecmp(name, "ucb1") == 0 || strcasecmp(name, "mcts") == 0) {
        return ENGINE_TYPE_MCTS_UCB1;
    }
    if (strcasecmp(name, "mcts_puct") == 0 || strcasecmp(name, "puct") == 0) {
        return ENGINE_TYPE_MCTS_PUCT;
    }
    if (strcasecmp(name, "checkerboard") == 0 || strcasecmp(name, "cb") == 0) {
        return ENGINE_TYPE_CHECKERBOARD;
    }
    if (strcasecmp(name, "kingsrow") == 0 || strcasecmp(name, "kr") == 0) {
        return ENGINE_TYPE_KINGSROW;
    }
    if (strcasecmp(name, "random") == 0 || strcasecmp(name, "rnd") == 0) {
        return ENGINE_TYPE_RANDOM;
    }
    return ENGINE_TYPE_COUNT;
}

typedef struct {
    int        thread_id;
    bool       is_active;
    int        game_index;
    EngineType white_engine;
    EngineType black_engine;
    Player     current_player;
    int        current_ply;
    int        white_pieces;
    int        black_pieces;
    double     game_start_time;
} ActiveThreadStatus;

static void format_duration(double seconds, char *buf, size_t buf_size) {
    if (seconds < 0.0) seconds = 0.0;
    int total_sec = (int)seconds;
    int hrs = total_sec / 3600;
    int mins = (total_sec % 3600) / 60;
    int secs = total_sec % 60;
    int tenths = (int)((seconds - (double)total_sec) * 10.0);

    if (hrs > 0) {
        snprintf(buf, buf_size, "%02d:%02d:%02d", hrs, mins, secs);
    } else if (mins > 0) {
        snprintf(buf, buf_size, "%02d:%02d.%1d", mins, secs, tenths);
    } else {
        snprintf(buf, buf_size, "%02d.%1ds", secs, tenths);
    }
}

static int s_prev_dashboard_lines = 0;

static void render_live_dashboard(int completed_games, int total_games, double start_time,
                                  int num_threads, const ActiveThreadStatus *thread_statuses) {
    double now = cli_get_time();
    double elapsed_total = now - start_time;

    // Calculate ETA and estimated total duration
    double eta = 0.0;
    double estimated_total = 0.0;
    if (completed_games > 0) {
        double avg_per_game = elapsed_total / (double)completed_games;
        int remaining = total_games - completed_games;
        eta = avg_per_game * (double)remaining;
        estimated_total = elapsed_total + eta;
    }

    char str_elapsed[32], str_eta[32], str_total[32];
    format_duration(elapsed_total, str_elapsed, sizeof(str_elapsed));
    format_duration(eta, str_eta, sizeof(str_eta));
    format_duration(estimated_total, str_total, sizeof(str_total));

    // Erase previously printed lines
    if (s_prev_dashboard_lines > 0) {
        fprintf(stderr, "\033[%dA\033[J", s_prev_dashboard_lines);
    }

    int lines_printed = 0;

    // Line 1: Main Progress Bar
    int bar_width = 28;
    float fraction = (float)completed_games / (float)(total_games > 0 ? total_games : 1);
    int filled = (int)(fraction * (float)bar_width);
    if (filled > bar_width) filled = bar_width;

    fprintf(stderr, "[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) fprintf(stderr, "=");
        else if (i == filled) fprintf(stderr, ">");
        else fprintf(stderr, " ");
    }
    fprintf(stderr, "] %3d/%-3d games (%5.1f%%)\n", completed_games, total_games, fraction * 100.0f);
    lines_printed++;

    // Line 2: Global Timing Stats (Elapsed, ETA, Est Total, Threads)
    if (completed_games > 0) {
        fprintf(stderr, "  \033[1mElapsed:\033[0m %-8s | \033[1mETA:\033[0m %-8s | \033[1mEst. Total:\033[0m %-8s | \033[1mThreads:\033[0m %d\n",
                str_elapsed, str_eta, str_total, num_threads);
    } else {
        fprintf(stderr, "  \033[1mElapsed:\033[0m %-8s | \033[1mETA:\033[0m Estimating... | \033[1mEst. Total:\033[0m --:--   | \033[1mThreads:\033[0m %d\n",
                str_elapsed, num_threads);
    }
    lines_printed++;

    // Lines 3..3+num_threads: Per-thread status
    for (int t = 0; t < num_threads; t++) {
        const ActiveThreadStatus *st = &thread_statuses[t];
        const char *tree_char = (t == num_threads - 1) ? "└─" : "├─";

        if (st && st->is_active) {
            double game_elapsed = now - st->game_start_time;
            char str_game_time[32];
            format_duration(game_elapsed, str_game_time, sizeof(str_game_time));

            const char *w_name = engine_get_type_name(st->white_engine);
            const char *b_name = engine_get_type_name(st->black_engine);
            const char *turn_str = (st->current_player == PLAYER_WHITE) ? "White" : "Black";

            fprintf(stderr, "  %s [Thread %2d] Game #%-3d: %s vs %s | Ply %3d (W:%2d B:%2d) | Turn: %-5s | Time: %s\n",
                    tree_char, t + 1, st->game_index, w_name, b_name,
                    st->current_ply, st->white_pieces, st->black_pieces, turn_str, str_game_time);
        } else {
            fprintf(stderr, "  %s [Thread %2d] Idle / Finished\n", tree_char, t + 1);
        }
        lines_printed++;
    }

    s_prev_dashboard_lines = lines_printed;
    fflush(stderr);
}

static void clear_live_dashboard(void) {
    if (s_prev_dashboard_lines > 0) {
        fprintf(stderr, "\033[%dA\033[J", s_prev_dashboard_lines);
        s_prev_dashboard_lines = 0;
        fflush(stderr);
    }
}

static void print_help(const char *prog_name) {
    printf("==============================================================================\n");
    printf("  Damascus 3D - Italian Draughts (Dama Italiana FID) Headless CLI Engine\n");
    printf("==============================================================================\n\n");
    printf("USAGE:\n");
    printf("  %s [MODE] [OPTIONS]\n\n", prog_name);
    printf("MODES:\n");
    printf("  --match               Run a head-to-head match between two engines.\n");
    printf("  --tournament          Run a Round-Robin tournament across multiple engines.\n");
    printf("  --bench               Run speed, throughput, and MCTS search benchmarks.\n");
    printf("  --gui                 Launch the OpenGL/GLFW 3D graphical interface.\n");
    printf("  --help, -h            Show this help manual.\n\n");
    printf("MATCH & TOURNAMENT OPTIONS:\n");
    printf("  --white=<engine>      White engine: ucb1, puct, checkerboard, kingsrow, random (default: ucb1)\n");
    printf("  --black=<engine>      Black engine: ucb1, puct, checkerboard, kingsrow, random (default: puct)\n");
    printf("  --engines=<list>      Comma-separated engines for tournament, or 'all'\n");
    printf("                        Example: --engines=ucb1,puct,checkerboard,random\n");
    printf("  --time=<seconds>, -T  Time budget per move in seconds (default: 1.0 for match, 0.2 for tournament)\n");
    printf("  --white-time=<sec>    Asymmetric time budget for White engine\n");
    printf("  --black-time=<sec>    Asymmetric time budget for Black engine\n");
    printf("  --games=<N>, -n       Total number of games for match (default: 10)\n");
    printf("  --games-per-pair=<N>  Games per pairing in tournament (default: 20)\n");
    printf("  --threads=<N>, -j     Number of concurrent worker threads (e.g. --threads=4 or --threads=auto, default: 1)\n");
    printf("  --max-plies=<N>       Max plies before declaring draw (default: 250)\n");
    printf("  --opening-plies=<N>   Random opening plies for book diversity (default: 2)\n");
    printf("  --csv=<filepath>, -o  Filepath to export match/tournament/benchmark results in CSV format\n");
    printf("  --quiet, -q           Suppress real-time progress ticker on stderr\n");
    printf("  --verbose, -v         Print move-by-move notation and detailed logs\n\n");
    printf("HYPERPARAMETER TUNING OPTIONS:\n");
    printf("  --alpha=<float>       UCB1 exploration constant alpha (default: 1.414)\n");
    printf("  --c-puct=<float>      PUCT exploration constant c_puct (default: 1.5)\n");
    printf("  --tau=<float>         PUCT softmax temperature (default: 1.0)\n");
    printf("  --epsilon=<float>     Biased rollout exploration rate (default: 0.15)\n");
    printf("  --rollout-depth=<int> Max simulation rollout depth (default: 70)\n");
    printf("  --no-db               Disable WLD endgame database during search\n\n");
    printf("BENCHMARK OPTIONS:\n");
    printf("  --budget=<list>       Comma-separated time budgets in seconds (default: 0.2,1.0,3.0)\n\n");
    printf("EXAMPLES:\n");
    printf("  # Single match between UCB1 and PUCT (10 games, 1.0s/move, 4 threads):\n");
    printf("  %s --match --white=ucb1 --black=puct --time=1.0 --games=10 --threads=4 --csv=results/match.csv\n\n", prog_name);
    printf("  # Parallel Round-Robin Tournament across 4 engines (8 threads):\n");
    printf("  %s --tournament --engines=ucb1,puct,checkerboard,random --time=0.2 --games-per-pair=20 --threads=8\n\n", prog_name);
    printf("  # Speed and throughput benchmark:\n");
    printf("  %s --bench --budget=0.2,1.0,3.0 --csv=results/bench.csv\n\n", prog_name);
}

static void init_default_cli_config(CliConfig *cfg) {
    memset(cfg, 0, sizeof(CliConfig));
    cfg->mode = CLI_MODE_GUI;
    cfg->white_engine = ENGINE_TYPE_MCTS_UCB1;
    cfg->black_engine = ENGINE_TYPE_MCTS_PUCT;
    
    cfg->tournament_engine_count = 4;
    cfg->tournament_engines[0] = ENGINE_TYPE_MCTS_UCB1;
    cfg->tournament_engines[1] = ENGINE_TYPE_MCTS_PUCT;
    cfg->tournament_engines[2] = ENGINE_TYPE_CHECKERBOARD;
    cfg->tournament_engines[3] = ENGINE_TYPE_RANDOM;
    
    cfg->time_budget = 1.0;
    cfg->white_time_budget = 0.0;
    cfg->black_time_budget = 0.0;
    cfg->games = 10;
    cfg->games_per_pair = 20;
    cfg->threads = 1;
    cfg->max_plies = DEFAULT_MAX_PLIES;
    cfg->opening_plies = DEFAULT_OPENING_PLIES;
    
    cfg->bench_budget_count = 3;
    cfg->bench_budgets[0] = 0.20;
    cfg->bench_budgets[1] = 1.00;
    cfg->bench_budgets[2] = 3.00;
    
    engine_config_init_default(&cfg->engine_config);
    cfg->quiet = false;
    cfg->verbose = false;
    cfg->csv_path[0] = '\0';
}

static bool parse_cli_args(int argc, char **argv, CliConfig *cfg) {
    init_default_cli_config(cfg);
    if (argc <= 1) return true;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            cfg->mode = CLI_MODE_HELP;
            return true;
        } else if (strcmp(arg, "--match") == 0 || strcmp(arg, "-m") == 0) {
            cfg->mode = CLI_MODE_MATCH;
        } else if (strcmp(arg, "--tournament") == 0 || strcmp(arg, "-t") == 0) {
            cfg->mode = CLI_MODE_TOURNAMENT;
            if (cfg->time_budget == 1.0) cfg->time_budget = 0.20; // Default fast budget for tournaments
        } else if (strcmp(arg, "--bench") == 0 || strcmp(arg, "-b") == 0) {
            cfg->mode = CLI_MODE_BENCH;
        } else if (strcmp(arg, "--gui") == 0) {
            cfg->mode = CLI_MODE_GUI;
        } else if (strncmp(arg, "--white=", 8) == 0) {
            EngineType t = parse_engine_name(arg + 8);
            if (t == ENGINE_TYPE_COUNT) {
                fprintf(stderr, "Error: Unknown white engine '%s'\n", arg + 8);
                return false;
            }
            cfg->white_engine = t;
        } else if (strncmp(arg, "--black=", 8) == 0) {
            EngineType t = parse_engine_name(arg + 8);
            if (t == ENGINE_TYPE_COUNT) {
                fprintf(stderr, "Error: Unknown black engine '%s'\n", arg + 8);
                return false;
            }
            cfg->black_engine = t;
        } else if (strncmp(arg, "--engines=", 10) == 0) {
            const char *eng_str = arg + 10;
            if (strcasecmp(eng_str, "all") == 0) {
                cfg->tournament_engine_count = 0;
                for (int t = 0; t < ENGINE_TYPE_COUNT; t++) {
                    if (engine_is_type_available((EngineType)t)) {
                        cfg->tournament_engines[cfg->tournament_engine_count++] = (EngineType)t;
                    }
                }
            } else {
                cfg->tournament_engine_count = 0;
                char buf[256];
                snprintf(buf, sizeof(buf), "%s", eng_str);
                char *token = strtok(buf, ",;");
                while (token && cfg->tournament_engine_count < ENGINE_TYPE_COUNT) {
                    EngineType t = parse_engine_name(token);
                    if (t != ENGINE_TYPE_COUNT) {
                        cfg->tournament_engines[cfg->tournament_engine_count++] = t;
                    } else {
                        fprintf(stderr, "Warning: Ignoring unrecognized engine in list: '%s'\n", token);
                    }
                    token = strtok(NULL, ",;");
                }
            }
        } else if (strncmp(arg, "--time=", 7) == 0 || strncmp(arg, "-T=", 4) == 0) {
            const char *val = (arg[1] == 'T') ? arg + 4 : arg + 7;
            cfg->time_budget = atof(val);
        } else if (strcmp(arg, "-T") == 0 && i + 1 < argc) {
            cfg->time_budget = atof(argv[++i]);
        } else if (strncmp(arg, "--white-time=", 13) == 0) {
            cfg->white_time_budget = atof(arg + 13);
        } else if (strncmp(arg, "--black-time=", 13) == 0) {
            cfg->black_time_budget = atof(arg + 13);
        } else if (strncmp(arg, "--games=", 8) == 0 || strncmp(arg, "-n=", 4) == 0) {
            const char *val = (arg[1] == 'n') ? arg + 4 : arg + 8;
            cfg->games = atoi(val);
        } else if (strcmp(arg, "-n") == 0 && i + 1 < argc) {
            cfg->games = atoi(argv[++i]);
        } else if (strncmp(arg, "--games-per-pair=", 17) == 0) {
            cfg->games_per_pair = atoi(arg + 17);
        } else if (strncmp(arg, "--threads=", 10) == 0) {
            if (strcasecmp(arg + 10, "auto") == 0) {
                cfg->threads = detect_cpu_cores();
            } else {
                cfg->threads = atoi(arg + 10);
            }
            if (cfg->threads < 1) cfg->threads = 1;
        } else if (strncmp(arg, "-j", 2) == 0) {
            if (arg[2] == '\0' && i + 1 < argc) {
                cfg->threads = atoi(argv[++i]);
            } else if (arg[2] != '\0') {
                cfg->threads = atoi(arg + 2);
            }
            if (cfg->threads < 1) cfg->threads = 1;
        } else if (strncmp(arg, "--max-plies=", 12) == 0) {
            cfg->max_plies = atoi(arg + 12);
        } else if (strncmp(arg, "--opening-plies=", 16) == 0) {
            cfg->opening_plies = atoi(arg + 16);
        } else if (strncmp(arg, "--budget=", 9) == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s", arg + 9);
            cfg->bench_budget_count = 0;
            char *tok = strtok(buf, ",;");
            while (tok && cfg->bench_budget_count < 8) {
                double b = atof(tok);
                if (b > 0.0) {
                    cfg->bench_budgets[cfg->bench_budget_count++] = b;
                }
                tok = strtok(NULL, ",;");
            }
        } else if (strncmp(arg, "--csv=", 6) == 0 || strncmp(arg, "-o=", 4) == 0) {
            const char *val = (arg[1] == 'o') ? arg + 4 : arg + 6;
            snprintf(cfg->csv_path, sizeof(cfg->csv_path), "%s", val);
        } else if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            snprintf(cfg->csv_path, sizeof(cfg->csv_path), "%s", argv[++i]);
        } else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0) {
            cfg->quiet = true;
        } else if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0) {
            cfg->verbose = true;
        } else if (strncmp(arg, "--alpha=", 8) == 0) {
            cfg->engine_config.mcts_exploration = (float)atof(arg + 8);
        } else if (strncmp(arg, "--c-puct=", 9) == 0) {
            cfg->engine_config.puct_c_puct = (float)atof(arg + 9);
        } else if (strncmp(arg, "--tau=", 6) == 0 || strncmp(arg, "--temperature=", 14) == 0) {
            const char *val = (arg[2] == 'u') ? arg + 6 : arg + 14;
            cfg->engine_config.puct_temperature = (float)atof(val);
        } else if (strncmp(arg, "--epsilon=", 10) == 0) {
            float eps = (float)atof(arg + 10);
            cfg->engine_config.mcts_rollout_epsilon = eps;
            cfg->engine_config.puct_rollout_epsilon = eps;
        } else if (strncmp(arg, "--rollout-depth=", 16) == 0) {
            int depth = atoi(arg + 16);
            cfg->engine_config.mcts_max_rollout_depth = depth;
            cfg->engine_config.puct_max_rollout_depth = depth;
        } else if (strcmp(arg, "--no-db") == 0) {
            cfg->engine_config.mcts_use_db = false;
            cfg->engine_config.puct_use_db = false;
        } else {
            fprintf(stderr, "Warning: Unrecognized argument '%s' (use --help for options)\n", arg);
        }
    }

    if (cfg->mode == CLI_MODE_GUI && argc > 1) {
        cfg->mode = CLI_MODE_MATCH;
    }
    return true;
}

static GameRecord play_single_game(int game_index, EngineType white_type, EngineType black_type,
                                   const CliConfig *cfg, int thread_id, int total_threads,
                                   int total_games, int *shared_completed_count,
                                   double tournament_start_time,
                                   ActiveThreadStatus *all_thread_statuses,
                                   pthread_mutex_t *display_mutex) {
    GameRecord record;
    memset(&record, 0, sizeof(record));
    record.game_index = game_index;
    record.white_engine = white_type;
    record.black_engine = black_type;

    ActiveThreadStatus *my_status = (all_thread_statuses && thread_id >= 0) ? &all_thread_statuses[thread_id] : NULL;

    if (my_status) {
        if (display_mutex) pthread_mutex_lock(display_mutex);
        my_status->thread_id = thread_id;
        my_status->is_active = true;
        my_status->game_index = game_index;
        my_status->white_engine = white_type;
        my_status->black_engine = black_type;
        my_status->current_player = PLAYER_WHITE;
        my_status->current_ply = 0;
        my_status->white_pieces = 12;
        my_status->black_pieces = 12;
        my_status->game_start_time = cli_get_time();
        if (!cfg->quiet) {
            int comp = shared_completed_count ? *shared_completed_count : 0;
            render_live_dashboard(comp, total_games, tournament_start_time, total_threads, all_thread_statuses);
        }
        if (display_mutex) pthread_mutex_unlock(display_mutex);
    }

    GameState game;
    game_init(&game, MODE_CPUVSCPU, PLAYER_WHITE, white_type, black_type);

    Engine white_eng = engine_create(white_type);
    Engine black_eng = engine_create(black_type);

    EngineConfig white_cfg = cfg->engine_config;
    EngineConfig black_cfg = cfg->engine_config;

    double w_time = (cfg->white_time_budget > 0.0) ? cfg->white_time_budget : cfg->time_budget;
    double b_time = (cfg->black_time_budget > 0.0) ? cfg->black_time_budget : cfg->time_budget;

    white_cfg.mcts_time_budget = w_time;
    white_cfg.puct_time_budget = w_time;
    white_cfg.cb_search_time   = w_time;
    white_cfg.kr_search_time   = w_time;

    black_cfg.mcts_time_budget = b_time;
    black_cfg.puct_time_budget = b_time;
    black_cfg.cb_search_time   = b_time;
    black_cfg.kr_search_time   = b_time;

    engine_apply_config(&white_eng, white_type, &white_cfg);
    engine_apply_config(&black_eng, black_type, &black_cfg);

    // Opening randomization plies
    int opening_plies = cfg->opening_plies;
    for (int p = 0; p < opening_plies && !game.is_game_over; p++) {
        const MoveList *legal = game_get_valid_moves(&game);
        if (!legal || legal->count == 0) break;
        int r_idx = rand() % legal->count;
        game_execute_move(&game, legal->moves[r_idx]);
        record.plies++;
    }

    double game_start = cli_get_time();

    while (!game.is_game_over && record.plies < cfg->max_plies) {
        Player current = game.current_player;
        Engine *active = (current == PLAYER_WHITE) ? &white_eng : &black_eng;

        if (my_status) {
            if (display_mutex) pthread_mutex_lock(display_mutex);
            my_status->current_ply = record.plies;
            my_status->current_player = current;
            my_status->white_pieces = __builtin_popcount(game.board.white_men | game.board.white_kings);
            my_status->black_pieces = __builtin_popcount(game.board.black_men | game.board.black_kings);
            if (!cfg->quiet) {
                int comp = shared_completed_count ? *shared_completed_count : 0;
                render_live_dashboard(comp, total_games, tournament_start_time, total_threads, all_thread_statuses);
            }
            if (display_mutex) pthread_mutex_unlock(display_mutex);
        }

        double move_start = cli_get_time();
        Move move = active->get_move(active->internal_state, &game);
        double move_time = cli_get_time() - move_start;

        if (current == PLAYER_WHITE) {
            record.white_total_time += move_time;
            record.white_move_count++;
        } else {
            record.black_total_time += move_time;
            record.black_move_count++;
        }

        if (move_is_none(move)) {
            // Forfeit / No legal moves
            game.is_game_over = true;
            game.is_draw = false;
            game.winner = (current == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
            snprintf(record.reason, sizeof(record.reason), "No legal moves (%s forfeit)",
                     current == PLAYER_WHITE ? "White" : "Black");
            break;
        }

        if (cfg->verbose) {
            printf("  [Ply %3d] %s plays %s (%d->%d)%s\n",
                   record.plies + 1,
                   current == PLAYER_WHITE ? "White" : "Black",
                   move.is_cap ? "CAPTURE" : "MOVE",
                   move.from, move.to,
                   move.is_prom ? " [PROMOTION]" : "");
        }

        game_execute_move(&game, move);
        record.plies++;
    }

    record.total_duration = cli_get_time() - game_start;
    record.white_pieces_remaining = __builtin_popcount(game.board.white_men | game.board.white_kings);
    record.black_pieces_remaining = __builtin_popcount(game.board.black_men | game.board.black_kings);

    if (game.is_game_over) {
        record.is_draw = game.is_draw;
        record.winner = game.winner;
        if (game.is_draw) {
            snprintf(record.reason, sizeof(record.reason), "3-Fold Repetition");
        } else {
            snprintf(record.reason, sizeof(record.reason), "Elimination / Block");
        }
    } else {
        record.is_draw = true;
        snprintf(record.reason, sizeof(record.reason), "Max Plies Limit (%d)", cfg->max_plies);
    }

    if (my_status) {
        if (display_mutex) pthread_mutex_lock(display_mutex);
        my_status->is_active = false;
        if (shared_completed_count) (*shared_completed_count)++;
        if (!cfg->quiet) {
            int comp = shared_completed_count ? *shared_completed_count : 0;
            render_live_dashboard(comp, total_games, tournament_start_time, total_threads, all_thread_statuses);
        }
        if (display_mutex) pthread_mutex_unlock(display_mutex);
    }

    engine_destroy(&white_eng);
    engine_destroy(&black_eng);
    return record;
}

static void export_match_csv(const char *csv_path, const GameRecord *records, int count, double time_budget) {
    if (!csv_path || !*csv_path) return;
    ensure_parent_dir_exists(csv_path);

    FILE *f = fopen(csv_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Unable to open CSV file '%s' for writing (%s)\n", csv_path, strerror(errno));
        return;
    }

    fprintf(f, "game_id,white_engine,black_engine,time_budget,winner_color,winner_engine,is_draw,reason,plies,white_avg_ms,black_avg_ms,duration_sec,white_pieces,black_pieces\n");
    for (int i = 0; i < count; i++) {
        const GameRecord *r = &records[i];
        double w_avg_ms = (r->white_move_count > 0) ? (r->white_total_time / r->white_move_count) * 1000.0 : 0.0;
        double b_avg_ms = (r->black_move_count > 0) ? (r->black_total_time / r->black_move_count) * 1000.0 : 0.0;
        const char *win_color = r->is_draw ? "Draw" : (r->winner == PLAYER_WHITE ? "White" : "Black");
        const char *win_eng = r->is_draw ? "Draw" : (r->winner == PLAYER_WHITE ? engine_get_type_name(r->white_engine) : engine_get_type_name(r->black_engine));

        fprintf(f, "%d,%s,%s,%.3f,%s,%s,%d,\"%s\",%d,%.2f,%.2f,%.3f,%d,%d\n",
                r->game_index,
                engine_get_type_name(r->white_engine),
                engine_get_type_name(r->black_engine),
                time_budget,
                win_color,
                win_eng,
                r->is_draw ? 1 : 0,
                r->reason,
                r->plies,
                w_avg_ms,
                b_avg_ms,
                r->total_duration,
                r->white_pieces_remaining,
                r->black_pieces_remaining);
    }
    fclose(f);
    printf("Match results successfully exported to CSV: %s\n", csv_path);
}

typedef struct {
    int game_index;
    EngineType white_engine;
    EngineType black_engine;
    int pair_i;
    int pair_j;
    bool a_is_white;
} GameJob;

typedef struct {
    int thread_id;
    int total_threads;
    int total_jobs;
    const CliConfig *cfg;
    const GameJob *jobs;
    GameRecord *records;
    int *next_job_index;
    int *jobs_completed;
    double tournament_start_time;
    ActiveThreadStatus *all_thread_statuses;
    pthread_mutex_t *mutex;
} WorkerContext;

static void *worker_thread_func(void *arg) {
    WorkerContext *ctx = (WorkerContext*)arg;
    while (true) {
        int job_idx = -1;
        pthread_mutex_lock(ctx->mutex);
        if (*ctx->next_job_index < ctx->total_jobs) {
            job_idx = (*ctx->next_job_index)++;
        }
        pthread_mutex_unlock(ctx->mutex);

        if (job_idx < 0) break; // No more jobs

        const GameJob *job = &ctx->jobs[job_idx];
        GameRecord rec = play_single_game(job->game_index, job->white_engine, job->black_engine,
                                          ctx->cfg, ctx->thread_id, ctx->total_threads,
                                          ctx->total_jobs, ctx->jobs_completed,
                                          ctx->tournament_start_time,
                                          ctx->all_thread_statuses, ctx->mutex);

        ctx->records[job_idx] = rec;
    }
    return NULL;
}

static int run_match_mode(const CliConfig *cfg) {
    int num_games = cfg->games;
    if (num_games > CLI_MAX_GAMES) num_games = CLI_MAX_GAMES;

    int num_threads = cfg->threads > 0 ? cfg->threads : 1;
    if (num_threads > num_games) num_threads = num_games;

    printf("==============================================================================\n");
    printf("  Damascus Headless Match Runner: %s vs %s\n",
           engine_get_type_name(cfg->white_engine), engine_get_type_name(cfg->black_engine));
    printf("  Games: %d | Time Budget: %.2fs | Threads: %d | Max Plies: %d | Opening: %d plies\n",
           num_games, cfg->time_budget, num_threads, cfg->max_plies, cfg->opening_plies);
    printf("==============================================================================\n\n");

    GameJob *jobs = (GameJob*)malloc(sizeof(GameJob) * num_games);
    GameRecord *records = (GameRecord*)malloc(sizeof(GameRecord) * num_games);
    if (!jobs || !records) {
        fprintf(stderr, "Memory allocation failed for match jobs/records\n");
        if (jobs) free(jobs);
        if (records) free(records);
        return 1;
    }

    for (int i = 0; i < num_games; i++) {
        bool swap = (i % 2 == 1);
        jobs[i].game_index = i + 1;
        jobs[i].white_engine = swap ? cfg->black_engine : cfg->white_engine;
        jobs[i].black_engine = swap ? cfg->white_engine : cfg->black_engine;
        jobs[i].pair_i = 0;
        jobs[i].pair_j = 0;
        jobs[i].a_is_white = !swap;
    }

    int next_job_idx = 0;
    int jobs_completed = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    ActiveThreadStatus *thread_statuses = (ActiveThreadStatus*)calloc(num_threads, sizeof(ActiveThreadStatus));

    double match_start = cli_get_time();

    if (num_threads > 1) {
        pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
        WorkerContext *ctxs = (WorkerContext*)malloc(sizeof(WorkerContext) * num_threads);

        for (int t = 0; t < num_threads; t++) {
            ctxs[t].thread_id = t;
            ctxs[t].total_threads = num_threads;
            ctxs[t].total_jobs = num_games;
            ctxs[t].cfg = cfg;
            ctxs[t].jobs = jobs;
            ctxs[t].records = records;
            ctxs[t].next_job_index = &next_job_idx;
            ctxs[t].jobs_completed = &jobs_completed;
            ctxs[t].tournament_start_time = match_start;
            ctxs[t].all_thread_statuses = thread_statuses;
            ctxs[t].mutex = &mutex;
            pthread_create(&threads[t], NULL, worker_thread_func, &ctxs[t]);
        }

        for (int t = 0; t < num_threads; t++) {
            pthread_join(threads[t], NULL);
        }

        free(threads);
        free(ctxs);
    } else {
        for (int i = 0; i < num_games; i++) {
            records[i] = play_single_game(jobs[i].game_index, jobs[i].white_engine, jobs[i].black_engine,
                                          cfg, 0, 1, num_games, &jobs_completed,
                                          match_start, thread_statuses, &mutex);
        }
    }

    if (!cfg->quiet) clear_live_dashboard();
    pthread_mutex_destroy(&mutex);
    free(thread_statuses);
    double match_duration = cli_get_time() - match_start;

    // Aggregate statistics
    int white_wins = 0;
    int black_wins = 0;
    int draws = 0;
    double total_plies = 0;

    for (int i = 0; i < num_games; i++) {
        if (records[i].is_draw) {
            draws++;
        } else {
            EngineType winner_engine = (records[i].winner == PLAYER_WHITE) ? records[i].white_engine : records[i].black_engine;
            if (winner_engine == cfg->white_engine) {
                white_wins++;
            } else {
                black_wins++;
            }
        }
        total_plies += records[i].plies;
    }

    // Summary Table Output
    printf("\n+----------------------------------------------------------------------------+\n");
    printf("| MATCH SUMMARY RESULTS                                                      |\n");
    printf("+----------------------------------------------------------------------------+\n");
    printf("  Engine 1 (%s): %d wins (%5.1f%%)\n",
           engine_get_type_name(cfg->white_engine), white_wins, ((float)white_wins / num_games) * 100.0f);
    printf("  Engine 2 (%s): %d wins (%5.1f%%)\n",
           engine_get_type_name(cfg->black_engine), black_wins, ((float)black_wins / num_games) * 100.0f);
    printf("  Draws:             %d      (%5.1f%%)\n",
           draws, ((float)draws / num_games) * 100.0f);
    printf("  Total Games:       %d\n", num_games);
    printf("  Average Plies:     %.1f plies/game\n", total_plies / num_games);
    printf("  Total Duration:    %.2f seconds (%.2f s/game across %d threads)\n", match_duration, match_duration / num_games, num_threads);
    printf("+----------------------------------------------------------------------------+\n\n");

    // Game by game breakdown
    printf("GAME-BY-GAME LOG:\n");
    printf("  # | White Engine      | Black Engine      | Winner            | Plies | Duration | Reason\n");
    printf("----+-------------------+-------------------+-------------------+-------+----------+--------------------\n");
    for (int i = 0; i < num_games; i++) {
        const GameRecord *r = &records[i];
        const char *w_name = engine_get_type_name(r->white_engine);
        const char *b_name = engine_get_type_name(r->black_engine);
        const char *res_str = r->is_draw ? "Draw" : (r->winner == PLAYER_WHITE ? w_name : b_name);

        printf("%3d | %-17s | %-17s | %-17s | %5d | %7.2fs | %s\n",
               r->game_index, w_name, b_name, res_str, r->plies, r->total_duration, r->reason);
    }
    printf("----------------------------------------------------------------------------------------------------\n\n");

    if (cfg->csv_path[0]) {
        export_match_csv(cfg->csv_path, records, num_games, cfg->time_budget);
    }

    free(jobs);
    free(records);
    return 0;
}

static int run_tournament_mode(const CliConfig *cfg) {
    int count = cfg->tournament_engine_count;
    if (count < 2) {
        fprintf(stderr, "Error: Tournament requires at least 2 engines (got %d)\n", count);
        return 1;
    }

    int num_pairs = (count * (count - 1)) / 2;
    int half = cfg->games_per_pair / 2;
    if (half < 1) half = 1;
    int g_per_pair = half * 2;
    int total_games = num_pairs * g_per_pair;

    int num_threads = cfg->threads > 0 ? cfg->threads : 1;
    if (num_threads > total_games) num_threads = total_games;

    printf("==============================================================================\n");
    printf("  Damascus Headless Round-Robin Tournament\n");
    printf("  Engines: %d | Time Budget: %.2fs | Games Per Pair: %d | Threads: %d\n",
           count, cfg->time_budget, g_per_pair, num_threads);
    printf("==============================================================================\n\n");

    TournamentEngineStats stats[ENGINE_TYPE_COUNT];
    memset(stats, 0, sizeof(stats));

    for (int i = 0; i < count; i++) {
        stats[i].type = cfg->tournament_engines[i];
        snprintf(stats[i].name, sizeof(stats[i].name), "%s", engine_get_type_name(stats[i].type));
    }

    double cross_matrix[ENGINE_TYPE_COUNT][ENGINE_TYPE_COUNT];
    int cross_wins[ENGINE_TYPE_COUNT][ENGINE_TYPE_COUNT];
    int cross_draws[ENGINE_TYPE_COUNT][ENGINE_TYPE_COUNT];
    int cross_losses[ENGINE_TYPE_COUNT][ENGINE_TYPE_COUNT];
    memset(cross_matrix, 0, sizeof(cross_matrix));
    memset(cross_wins, 0, sizeof(cross_wins));
    memset(cross_draws, 0, sizeof(cross_draws));
    memset(cross_losses, 0, sizeof(cross_losses));

    GameJob *jobs = (GameJob*)malloc(sizeof(GameJob) * total_games);
    GameRecord *all_records = (GameRecord*)malloc(sizeof(GameRecord) * total_games);
    if (!jobs || !all_records) {
        fprintf(stderr, "Memory allocation failed for tournament jobs/records\n");
        if (jobs) free(jobs);
        if (all_records) free(all_records);
        return 1;
    }

    int job_idx = 0;
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            EngineType eng_a = cfg->tournament_engines[i];
            EngineType eng_b = cfg->tournament_engines[j];

            for (int g = 0; g < g_per_pair; g++) {
                bool a_is_white = (g % 2 == 0);
                jobs[job_idx].game_index = job_idx + 1;
                jobs[job_idx].white_engine = a_is_white ? eng_a : eng_b;
                jobs[job_idx].black_engine = a_is_white ? eng_b : eng_a;
                jobs[job_idx].pair_i = i;
                jobs[job_idx].pair_j = j;
                jobs[job_idx].a_is_white = a_is_white;
                job_idx++;
            }
        }
    }

    int next_job_idx = 0;
    int jobs_completed = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    ActiveThreadStatus *thread_statuses = (ActiveThreadStatus*)calloc(num_threads, sizeof(ActiveThreadStatus));

    double tourney_start = cli_get_time();

    if (num_threads > 1) {
        pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
        WorkerContext *ctxs = (WorkerContext*)malloc(sizeof(WorkerContext) * num_threads);

        for (int t = 0; t < num_threads; t++) {
            ctxs[t].thread_id = t;
            ctxs[t].total_threads = num_threads;
            ctxs[t].total_jobs = total_games;
            ctxs[t].cfg = cfg;
            ctxs[t].jobs = jobs;
            ctxs[t].records = all_records;
            ctxs[t].next_job_index = &next_job_idx;
            ctxs[t].jobs_completed = &jobs_completed;
            ctxs[t].tournament_start_time = tourney_start;
            ctxs[t].all_thread_statuses = thread_statuses;
            ctxs[t].mutex = &mutex;
            pthread_create(&threads[t], NULL, worker_thread_func, &ctxs[t]);
        }

        for (int t = 0; t < num_threads; t++) {
            pthread_join(threads[t], NULL);
        }

        free(threads);
        free(ctxs);
    } else {
        for (int i = 0; i < total_games; i++) {
            all_records[i] = play_single_game(jobs[i].game_index, jobs[i].white_engine, jobs[i].black_engine,
                                              cfg, 0, 1, total_games, &jobs_completed,
                                              tourney_start, thread_statuses, &mutex);
        }
    }

    if (!cfg->quiet) clear_live_dashboard();
    pthread_mutex_destroy(&mutex);
    free(thread_statuses);
    double tourney_duration = cli_get_time() - tourney_start;

    // Aggregate statistics across all finished games
    for (int k = 0; k < total_games; k++) {
        const GameJob *job = &jobs[k];
        const GameRecord *rec = &all_records[k];
        int i = job->pair_i;
        int j = job->pair_j;
        bool a_is_white = job->a_is_white;

        stats[i].games_played++;
        stats[j].games_played++;
        stats[i].total_plies += rec->plies;
        stats[j].total_plies += rec->plies;

        if (a_is_white) {
            stats[i].total_time += rec->white_total_time;
            stats[i].total_moves += rec->white_move_count;
            stats[j].total_time += rec->black_total_time;
            stats[j].total_moves += rec->black_move_count;
        } else {
            stats[j].total_time += rec->white_total_time;
            stats[j].total_moves += rec->white_move_count;
            stats[i].total_time += rec->black_total_time;
            stats[i].total_moves += rec->black_move_count;
        }

        if (rec->is_draw) {
            stats[i].draws++;
            stats[j].draws++;
            stats[i].points += 0.5;
            stats[j].points += 0.5;
            cross_matrix[i][j] += 0.5;
            cross_matrix[j][i] += 0.5;
            cross_draws[i][j]++;
            cross_draws[j][i]++;
        } else {
            Player winning_color = rec->winner;
            bool a_won = (a_is_white && winning_color == PLAYER_WHITE) || (!a_is_white && winning_color == PLAYER_BLACK);

            if (a_won) {
                stats[i].wins++;
                stats[j].losses++;
                stats[i].points += 1.0;
                cross_matrix[i][j] += 1.0;
                cross_wins[i][j]++;
                cross_losses[j][i]++;
            } else {
                stats[j].wins++;
                stats[i].losses++;
                stats[j].points += 1.0;
                cross_matrix[j][i] += 1.0;
                cross_wins[j][i]++;
                cross_losses[i][j]++;
            }
        }
    }

    for (int i = 0; i < count; i++) {
        if (stats[i].games_played > 0) {
            stats[i].score_pct = (stats[i].points / stats[i].games_played) * 100.0;
        }
    }

    // Sort leaderboard by points descending
    int rank[ENGINE_TYPE_COUNT];
    for (int i = 0; i < count; i++) rank[i] = i;
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (stats[rank[j]].points > stats[rank[i]].points) {
                int tmp = rank[i];
                rank[i] = rank[j];
                rank[j] = tmp;
            }
        }
    }

    // Print Leaderboard
    printf("\n+===================================================================================================+\n");
    printf("| TOURNAMENT STANDINGS & LEADERBOARD                                                                |\n");
    printf("+======+==================+========+=======+=======+========+=========+==========+================+\n");
    printf("| Rank | Engine Name      | Played | Wins  | Draws | Losses | Points  | Score %%  | Avg Move Time  |\n");
    printf("+------+------------------+--------+-------+-------+--------+---------+----------+----------------+\n");
    for (int r = 0; r < count; r++) {
        int idx = rank[r];
        double avg_ms = (stats[idx].total_moves > 0) ? (stats[idx].total_time / stats[idx].total_moves) * 1000.0 : 0.0;
        printf("| %4d | %-16s | %6d | %5d | %5d | %6d | %7.1f | %7.1f%% | %11.2f ms |\n",
               r + 1, stats[idx].name, stats[idx].games_played, stats[idx].wins, stats[idx].draws,
               stats[idx].losses, stats[idx].points, stats[idx].score_pct, avg_ms);
    }
    printf("+------+------------------+--------+-------+-------+--------+---------+----------+----------------+\n\n");

    // Print Cross-Table
    printf("CROSS-TABLE (Head-to-Head Points [W-D-L]):\n");
    printf("%-18s", "Engine");
    for (int j = 0; j < count; j++) {
        printf(" %-12s", stats[j].name);
    }
    printf("  Total\n");
    printf("----------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("%-18s", stats[i].name);
        for (int j = 0; j < count; j++) {
            if (i == j) {
                printf(" %-12s", "    x    ");
            } else {
                char cell[32];
                snprintf(cell, sizeof(cell), "%.1f (%d-%d-%d)", cross_matrix[i][j],
                         cross_wins[i][j], cross_draws[i][j], cross_losses[i][j]);
                printf(" %-12s", cell);
            }
        }
        printf("  %5.1f pts\n", stats[i].points);
    }
    printf("----------------------------------------------------------------------------------------------------\n");
    printf("Total Tournament Time: %.2f seconds (%.2f s/game, %d threads across %d games)\n\n",
           tourney_duration, tourney_duration / (total_games > 0 ? total_games : 1), num_threads, total_games);

    if (cfg->csv_path[0] && all_records) {
        export_match_csv(cfg->csv_path, all_records, total_games, cfg->time_budget);
    }

    free(jobs);
    free(all_records);
    return 0;
}

static uint64_t perft_dfs(const GameState *game, int depth) {
    if (depth == 0) return 1;
    const MoveList *legal = game_get_valid_moves(game);
    if (!legal || legal->count == 0) return 0;
    if (depth == 1) return legal->count;

    uint64_t nodes = 0;
    for (int i = 0; i < legal->count; i++) {
        GameState next = *game;
        game_execute_move(&next, legal->moves[i]);
        nodes += perft_dfs(&next, depth - 1);
    }
    return nodes;
}

static int run_benchmark_mode(const CliConfig *cfg) {
    printf("==============================================================================\n");
    printf("  Damascus Engine Benchmark & Throughput Suite\n");
    printf("==============================================================================\n\n");

    wld_db_init();
    zobrist_init();

    // 1. Move Generation & Perft Throughput
    printf("1. MOVE GENERATOR & LAW OF MAXIMUM (PERFT THROUGHPUT):\n");
    printf("------------------------------------------------------------------------------\n");
    GameState start_game;
    game_init(&start_game, MODE_CPUVSCPU, PLAYER_WHITE, ENGINE_TYPE_RANDOM, ENGINE_TYPE_RANDOM);

    for (int depth = 1; depth <= 7; depth++) {
        double t0 = cli_get_time();
        uint64_t nodes = perft_dfs(&start_game, depth);
        double elapsed = cli_get_time() - t0;
        double knps = (elapsed > 0.0) ? ((double)nodes / elapsed) / 1000.0 : 0.0;
        printf("  Depth %d: %12llu nodes in %7.3fs (%9.2f kN/s)\n",
               depth, (unsigned long long)nodes, elapsed, knps);
        if (elapsed > 2.0) break; // Keep benchmark quick
    }
    printf("\n");

    // 2. Biased vs Uniform Random Rollout Throughput
    printf("2. SIMULATION ROLLOUT THROUGHPUT:\n");
    printf("------------------------------------------------------------------------------\n");
    int rollout_trials = 25000;
    
    // Biased Rollout (epsilon = 0.15)
    double t_biased_0 = cli_get_time();
    uint32_t rng_state = 0x12345678U;
    for (int i = 0; i < rollout_trials; i++) {
        GameState sim = start_game;
        int d = 0;
        while (!sim.is_game_over && d < 70) {
            const MoveList *moves = game_get_valid_moves(&sim);
            if (!moves || moves->count == 0) break;
            Move best_m = mcts_select_biased_rollout_move(&sim, moves, 0.15f, &rng_state);
            game_execute_move(&sim, best_m);
            d++;
        }
    }
    double t_biased = cli_get_time() - t_biased_0;
    printf("  Biased Rollout (FID heuristics, eps=0.15): %6d sims in %6.3fs (%8.1f sims/s)\n",
           rollout_trials, t_biased, (double)rollout_trials / (t_biased > 0 ? t_biased : 1));

    // Pure Random Rollout (epsilon = 1.0)
    double t_rand_0 = cli_get_time();
    for (int i = 0; i < rollout_trials; i++) {
        GameState sim = start_game;
        int d = 0;
        while (!sim.is_game_over && d < 70) {
            const MoveList *moves = game_get_valid_moves(&sim);
            if (!moves || moves->count == 0) break;
            rng_state ^= rng_state << 13;
            rng_state ^= rng_state >> 17;
            rng_state ^= rng_state << 5;
            int r_idx = (int)(rng_state % moves->count);
            game_execute_move(&sim, moves->moves[r_idx]);
            d++;
        }
    }
    double t_rand = cli_get_time() - t_rand_0;
    printf("  Uniform Random Rollout (eps=1.00):          %6d sims in %6.3fs (%8.1f sims/s)\n\n",
           rollout_trials, t_rand, (double)rollout_trials / (t_rand > 0 ? t_rand : 1));

    // 3. MCTS Search Engines Benchmark (Iterations/sec across time budgets)
    printf("3. MCTS SEARCH ENGINES ITERATION THROUGHPUT:\n");
    printf("------------------------------------------------------------------------------\n");
    printf("  Model     | Budget | Search Time | Root Visits | Total Pool Nodes | Iterations/s\n");
    printf("------------+--------+-------------+-------------+------------------+-------------\n");

    for (int b = 0; b < cfg->bench_budget_count; b++) {
        double budget = cfg->bench_budgets[b];

        // UCB1
        Engine ucb1_eng = engine_create(ENGINE_TYPE_MCTS_UCB1);
        EngineConfig u_cfg;
        engine_config_init_default(&u_cfg);
        u_cfg.mcts_time_budget = budget;
        engine_apply_config(&ucb1_eng, ENGINE_TYPE_MCTS_UCB1, &u_cfg);

        double t0_u = cli_get_time();
        ucb1_eng.get_move(ucb1_eng.internal_state, &start_game);
        double dur_u = cli_get_time() - t0_u;
        uint32_t visits_u = engine_mcts_ucb1_get_root_visits(ucb1_eng.internal_state);
        uint32_t pool_u = engine_mcts_ucb1_get_node_count();
        double ips_u = (dur_u > 0.0) ? (double)visits_u / dur_u : 0.0;

        printf("  MCTS UCB1 | %5.2fs | %10.3fs | %11u | %16u | %10.1f\n",
               budget, dur_u, visits_u, pool_u, ips_u);
        engine_destroy(&ucb1_eng);

        // PUCT
        Engine puct_eng = engine_create(ENGINE_TYPE_MCTS_PUCT);
        EngineConfig p_cfg;
        engine_config_init_default(&p_cfg);
        p_cfg.puct_time_budget = budget;
        engine_apply_config(&puct_eng, ENGINE_TYPE_MCTS_PUCT, &p_cfg);

        double t0_p = cli_get_time();
        puct_eng.get_move(puct_eng.internal_state, &start_game);
        double dur_p = cli_get_time() - t0_p;
        uint32_t visits_p = engine_mcts_puct_get_root_visits(puct_eng.internal_state);
        uint32_t pool_p = engine_mcts_puct_get_node_count();
        double ips_p = (dur_p > 0.0) ? (double)visits_p / dur_p : 0.0;

        printf("  MCTS PUCT | %5.2fs | %10.3fs | %11u | %16u | %10.1f\n",
               budget, dur_p, visits_p, pool_p, ips_p);
        engine_destroy(&puct_eng);
    }
    printf("------------------------------------------------------------------------------\n\n");

    if (cfg->csv_path[0]) {
        ensure_parent_dir_exists(cfg->csv_path);
        FILE *f = fopen(cfg->csv_path, "w");
        if (f) {
            fprintf(f, "metric,model,budget_sec,time_elapsed_sec,visits_or_nodes,throughput_per_sec\n");
            fprintf(f, "rollout,biased_FID,0,%.4f,%d,%.1f\n", t_biased, rollout_trials, (double)rollout_trials / t_biased);
            fprintf(f, "rollout,uniform_random,0,%.4f,%d,%.1f\n", t_rand, rollout_trials, (double)rollout_trials / t_rand);
            fclose(f);
            printf("Benchmark results exported to CSV: %s\n", cfg->csv_path);
        }
    }

    return 0;
}

int cli_run(int argc, char **argv) {
    CliConfig cfg;
    if (!parse_cli_args(argc, argv, &cfg)) {
        return 1;
    }

    switch (cfg.mode) {
        case CLI_MODE_HELP:
            print_help(argv[0]);
            return 0;
        case CLI_MODE_MATCH:
            return run_match_mode(&cfg);
        case CLI_MODE_TOURNAMENT:
            return run_tournament_mode(&cfg);
        case CLI_MODE_BENCH:
            return run_benchmark_mode(&cfg);
        case CLI_MODE_GUI:
        default:
            return 0;
    }
}
