#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "tune_ga.h"
#include "engine.h"
#include "game.h"

static void test_chromosome_initialization(void) {
    printf("[1/5] Testing Chromosome Initialization & Defaults...\n");
    
    Chromosome c_puct;
    ga_chromosome_init_default(&c_puct, ENGINE_TYPE_MCTS_PUCT);
    assert(c_puct.target_engine == ENGINE_TYPE_MCTS_PUCT);
    assert(fabsf(c_puct.puct.c_puct - 2.5857f) < 1e-4f);
    assert(fabsf(c_puct.puct.puct_temperature - 2.2048f) < 1e-4f);
    assert(c_puct.puct.use_guided_book == true);
    assert(fabsf(c_puct.puct.lambda_book - 0.0791f) < 1e-4f);
    assert(c_puct.puct.book_mode == BOOK_MODE_OFF);
    assert(fabsf(c_puct.puct.book_temperature - 0.0f) < 1e-4f);
    assert(fabsf(c_puct.rollout_epsilon - 0.3416f) < 1e-4f);
    assert(c_puct.max_rollout_depth == 112);
    assert(c_puct.use_db == false);

    Chromosome c_ucb1;
    ga_chromosome_init_default(&c_ucb1, ENGINE_TYPE_MCTS_UCB1);
    assert(c_ucb1.target_engine == ENGINE_TYPE_MCTS_UCB1);
    assert(fabsf(c_ucb1.ucb1.exploration_alpha - 0.6422f) < 1e-4f);
    assert(c_ucb1.ucb1.book_mode == BOOK_MODE_GOOD);
    assert(fabsf(c_ucb1.ucb1.book_temperature - 0.0500f) < 1e-4f);
    assert(fabsf(c_ucb1.rollout_epsilon - 0.3128f) < 1e-4f);
    assert(c_ucb1.max_rollout_depth == 51);
    assert(c_ucb1.use_db == false);

    printf("  -> Baseline chromosome initialization: PASSED\n");
}

static void test_chromosome_randomization_and_sanitization(void) {
    printf("[2/5] Testing Chromosome Randomization, Clamping & Sanitization (1,000 samples)...\n");
    uint32_t rng = 0xDEADBEEF;

    for (int i = 0; i < 1000; i++) {
        Chromosome c;
        ga_chromosome_randomize(&c, (i % 2 == 0) ? ENGINE_TYPE_MCTS_PUCT : ENGINE_TYPE_MCTS_UCB1, &rng);
        
        assert(c.rollout_epsilon >= 0.01f && c.rollout_epsilon <= 0.50f);
        assert(c.max_rollout_depth >= 10 && c.max_rollout_depth <= 150);

        if (c.target_engine == ENGINE_TYPE_MCTS_PUCT) {
            assert(c.puct.c_puct >= 0.2f && c.puct.c_puct <= 5.0f);
            assert(c.puct.puct_temperature >= 0.05f && c.puct.puct_temperature <= 3.0f);
            if (c.puct.use_guided_book) {
                assert(c.puct.lambda_book >= 0.0f && c.puct.lambda_book <= 1.0f);
                assert(c.puct.book_mode == BOOK_MODE_OFF);
                assert(c.puct.book_temperature == 0.0f);
            } else {
                assert(c.puct.lambda_book == 0.0f);
                if (c.puct.book_mode == BOOK_MODE_OFF || c.puct.book_mode == BOOK_MODE_BEST) {
                    assert(c.puct.book_temperature == 0.0f);
                } else {
                    assert(c.puct.book_temperature >= 0.05f && c.puct.book_temperature <= 4.0f);
                }
            }
        } else {
            assert(c.ucb1.exploration_alpha >= 0.1f && c.ucb1.exploration_alpha <= 4.0f);
            if (c.ucb1.book_mode == BOOK_MODE_OFF || c.ucb1.book_mode == BOOK_MODE_BEST) {
                assert(c.ucb1.book_temperature == 0.0f);
            } else {
                assert(c.ucb1.book_temperature >= 0.05f && c.ucb1.book_temperature <= 4.0f);
            }
        }
    }
    printf("  -> Parameter bounds, clamping, and zero-ghost-gene sanitization: PASSED\n");
}

static void test_crossover_operator(void) {
    printf("[3/5] Testing Crossover Operator...\n");
    uint32_t rng = 0x55AA55AA;

    Chromosome p1, p2, child1, child2;
    ga_chromosome_init_default(&p1, ENGINE_TYPE_MCTS_PUCT);
    p1.puct.c_puct = 1.0f;
    p1.puct.puct_temperature = 0.5f;
    p1.rollout_epsilon = 0.10f;
    p1.max_rollout_depth = 50;

    ga_chromosome_init_default(&p2, ENGINE_TYPE_MCTS_PUCT);
    p2.puct.c_puct = 3.0f;
    p2.puct.puct_temperature = 2.0f;
    p2.rollout_epsilon = 0.40f;
    p2.max_rollout_depth = 120;

    ga_chromosome_crossover(&p1, &p2, &child1, &child2, 1.0f, &rng);

    assert(child1.target_engine == ENGINE_TYPE_MCTS_PUCT);
    assert(child2.target_engine == ENGINE_TYPE_MCTS_PUCT);
    assert(child1.puct.c_puct >= 0.2f && child1.puct.c_puct <= 5.0f);
    assert(child2.puct.c_puct >= 0.2f && child2.puct.c_puct <= 5.0f);

    printf("  -> Crossover inheritance and bound check: PASSED\n");
}

static void test_mutation_operator(void) {
    printf("[4/5] Testing Mutation Operator...\n");
    uint32_t rng = 0x12344321;

    Chromosome orig, mutated;
    ga_chromosome_init_default(&orig, ENGINE_TYPE_MCTS_PUCT);
    mutated = orig;

    ga_chromosome_mutate(&mutated, 1.0f, 0.20f, &rng);

    // Mutation with rate 1.0 must perturb parameters
    bool changed = (mutated.puct.c_puct != orig.puct.c_puct) ||
                   (mutated.puct.puct_temperature != orig.puct.puct_temperature) ||
                   (mutated.rollout_epsilon != orig.rollout_epsilon) ||
                   (mutated.max_rollout_depth != orig.max_rollout_depth) ||
                   (mutated.puct.lambda_book != orig.puct.lambda_book);
    assert(changed);
    assert(mutated.puct.c_puct >= 0.2f && mutated.puct.c_puct <= 5.0f);

    printf("  -> Mutation perturbation: PASSED\n");
}

static void test_mini_tuning_run(void) {
    printf("[5/5] Testing Mini GA Tuning Execution (Pop: 4, Gen: 2, Sequential Mode)...\n");

    GAConfig cfg;
    ga_config_init_default(&cfg, ENGINE_TYPE_MCTS_PUCT);
    cfg.population_size = 4;
    cfg.generations = 2;
    cfg.games_per_pair = 2;
    cfg.time_budget = 0.01; // Ultra fast for unit test
    cfg.max_plies = 20;     // Truncated for speedy validation
    cfg.threads = 1;
    cfg.quiet = true;
    cfg.seed = 42;

    GAResult res;
    int ret = tune_ga_run(&cfg, &res);
    assert(ret == 0);
    assert(res.total_games_played == 24); // (4*3/2)*2 games * 2 gens = 24 games
    assert(res.best_overall.games_played > 0);

    printf("  -> Mini GA Tuning Run (24 games): PASSED\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    zobrist_init();
    wld_db_init();
    opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);

    printf("==============================================================================\n");
    printf("  Damascus Unit Test Suite - Genetic Algorithm Tuning Subsystem\n");
    printf("==============================================================================\n\n");

    test_chromosome_initialization();
    test_chromosome_randomization_and_sanitization();
    test_crossover_operator();
    test_mutation_operator();
    test_mini_tuning_run();

    printf("\n>>> ALL 5 GA TUNING UNIT TESTS PASSED SUCCESSFULLY (100%%) <<<\n\n");
    return 0;
}
