#ifndef CLI_H
#define CLI_H

#include "game.h"
#include "engine.h"
#include <stdbool.h>

typedef enum {
    CLI_MODE_GUI = 0,
    CLI_MODE_HELP,
    CLI_MODE_MATCH,
    CLI_MODE_TOURNAMENT,
    CLI_MODE_BENCH,
    CLI_MODE_TEST_ENDGAMES,
    CLI_MODE_TEST_OPENING_BOOK,
    CLI_MODE_TEST_OPENING_TOURNAMENT
} CliMode;

typedef struct {
    CliMode    mode;
    
    // Engine selections
    EngineType white_engine;
    EngineType black_engine;
    EngineType tournament_engines[ENGINE_TYPE_COUNT];
    int        tournament_engine_count;
    
    // Time & game settings
    double     time_budget;        // Default time budget per move (seconds)
    double     white_time_budget;  // Custom white time budget (0 = use time_budget)
    double     black_time_budget;  // Custom black time budget (0 = use time_budget)
    int        games;              // Number of games for match
    int        games_per_pair;     // Number of games per pair in tournament
    int        max_plies;          // Maximum plies per game before draw
    int        opening_plies;      // Number of random opening plies for variation
    
    // Benchmark options
    double     bench_budgets[8];
    int        bench_budget_count;
    
    // Hyperparameters & WLD / Book Options
    EngineConfig engine_config;
    WLDBackendType wld_backend;
    char       wld_path[256];
    
    // Player-specific book overrides (for match & tournament comparisons)
    bool       has_white_book_override;
    bool       white_use_book;
    BookPlayMode white_book_mode;
    bool       has_black_book_override;
    bool       black_use_book;
    BookPlayMode black_book_mode;

    // Output & logging
    char       csv_path[256];
    int        threads;            // Number of concurrent worker threads (default: 1)
    bool       quiet;              // Disable progress bar / ticker on stderr
    bool       verbose;            // Print move-by-move details
} CliConfig;

// Main CLI entry point: returns 0 on success, non-zero on error
int cli_run(int argc, char **argv);

#endif // CLI_H
