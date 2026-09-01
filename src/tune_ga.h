#ifndef TUNE_GA_H
#define TUNE_GA_H

#include "game.h"
#include "engine.h"
#include <stdbool.h>
#include <stdint.h>

#define GA_MAX_POPULATION 64
#define GA_DEFAULT_POPULATION 16
#define GA_DEFAULT_GENERATIONS 5
#define GA_DEFAULT_GAMES_PER_PAIR 2
#define GA_DEFAULT_TIME_BUDGET 0.20
#define GA_DEFAULT_MUTATION_RATE 0.20f
#define GA_DEFAULT_MUTATION_SCALE 0.15f
#define GA_DEFAULT_CROSSOVER_RATE 0.80f
#define GA_DEFAULT_ELITE_COUNT 2

// Dedicated Genome for MCTS UCB1
typedef struct {
    float        exploration_alpha;     // Alpha exploration constant [0.2, 3.5]
    BookPlayMode book_mode;             // BOOK_MODE_OFF, BOOK_MODE_BEST, BOOK_MODE_GOOD, BOOK_MODE_ALL
    float        book_temperature;      // Temperature for move sampling [0.1, 3.0] (active only if GOOD or ALL)
} UCB1Genome;

// Dedicated Genome for MCTS PUCT
typedef struct {
    float        c_puct;                // Exploration constant c_puct [0.5, 5.0]
    float        puct_temperature;      // Policy Softmax temperature tau [0.1, 3.0]
    bool         use_guided_book;       // PUCT prior blending toggle
    float        lambda_book;           // Blending factor lambda in [0.0, 1.0] (active only if use_guided_book)
    BookPlayMode book_mode;             // Instant book mode (active only if !use_guided_book)
    float        book_temperature;      // Temperature [0.1, 3.0] (active only if !use_guided_book && GOOD/ALL)
} PUCTGenome;

typedef struct {
    EngineType target_engine; // ENGINE_TYPE_MCTS_PUCT or ENGINE_TYPE_MCTS_UCB1

    // Common MCTS / Simulation parameters
    float      rollout_epsilon;         // [0.01, 0.50]
    int        max_rollout_depth;       // [10, 150]
    bool       use_db;                  // Endgame Tablebase parameter
    
    // Engine-Specific Genomes (Tagged Union)
    union {
        UCB1Genome ucb1;
        PUCTGenome puct;
    };

    // Fitness and Tournament evaluation stats
    int        id;
    int        generation;
    int        games_played;
    int        wins;
    int        draws;
    int        losses;
    double     points;                  // Win = 1.0, Draw = 0.5, Loss = 0.0
    double     score_pct;               // points / games_played * 100.0
    double     elo;                     // Estimated Bayes/Tournament Elo relative to base 1500
    double     total_time_spent;
    int        total_moves;
    double     fitness;                 // Composite fitness score
} Chromosome;

typedef struct {
    Chromosome individuals[GA_MAX_POPULATION];
    int        size;
    int        generation;
    int        best_idx;
} Population;

typedef struct {
    EngineType target_engine;
    int        population_size;
    int        generations;
    int        games_per_pair;
    double     time_budget;
    int        max_plies;
    int        opening_plies;
    float      mutation_rate;
    float      mutation_scale;
    float      crossover_rate;
    int        elite_count;
    int        threads;
    char       csv_path[256];
    bool       quiet;
    bool       verbose;
    uint32_t   seed;
} GAConfig;

typedef struct {
    Chromosome best_overall;
    int        total_games_played;
    double     total_duration;
    double     initial_best_score;
    double     final_best_score;
    double     initial_best_elo;
    double     final_best_elo;
} GAResult;

// GA Core API
void ga_config_init_default(GAConfig *cfg, EngineType target_engine);
void ga_chromosome_init_default(Chromosome *c, EngineType target_engine);
void ga_chromosome_sanitize(Chromosome *c);
void ga_chromosome_randomize(Chromosome *c, EngineType target_engine, uint32_t *rng);
void ga_chromosome_clamp(Chromosome *c);
void ga_chromosome_apply_to_config(const Chromosome *c, EngineConfig *cfg);
void ga_chromosome_crossover(const Chromosome *p1, const Chromosome *p2, Chromosome *child1, Chromosome *child2, float crossover_rate, uint32_t *rng);
void ga_chromosome_mutate(Chromosome *c, float mutation_rate, float mutation_scale, uint32_t *rng);

// GA Population & Evolution Lifecycle
void ga_population_init(Population *pop, const GAConfig *cfg, uint32_t *rng);
void ga_population_evaluate(Population *pop, const GAConfig *cfg);
void ga_population_evolve(Population *pop, const GAConfig *cfg, uint32_t *rng);

// High-level Runner
int tune_ga_run(const GAConfig *cfg, GAResult *out_result);

#endif // TUNE_GA_H
