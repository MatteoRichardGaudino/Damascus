#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "tune_ga.h"
#include "game.h"
#include "engine.h"
#include "mcts_ucb1.h"
#include "mcts_puct.h"
#include "wld_db.h"
#include "opening_book.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(d) _mkdir(d)
#endif

#if defined(__GNUC__) || !defined(_MSC_VER)
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
typedef pthread_mutex_t ga_mutex_t;
#define ga_mutex_init(m) pthread_mutex_init(m, NULL)
#define ga_mutex_lock(m) pthread_mutex_lock(m)
#define ga_mutex_unlock(m) pthread_mutex_unlock(m)
#define ga_mutex_destroy(m) pthread_mutex_destroy(m)

typedef pthread_t ga_thread_t;
static inline int ga_thread_create(ga_thread_t *thread, void *(*start_routine)(void *), void *arg) {
    return pthread_create(thread, NULL, start_routine, arg);
}
static inline int ga_thread_join(ga_thread_t thread) {
    return pthread_join(thread, NULL);
}
#else
typedef CRITICAL_SECTION ga_mutex_t;
#define ga_mutex_init(m) InitializeCriticalSection(m)
#define ga_mutex_lock(m) EnterCriticalSection(m)
#define ga_mutex_unlock(m) LeaveCriticalSection(m)
#define ga_mutex_destroy(m) DeleteCriticalSection(m)

typedef HANDLE ga_thread_t;
typedef void* (*ga_thread_func)(void*);
typedef struct {
    ga_thread_func func;
    void *arg;
} ga_win_thread_arg_t;

static DWORD WINAPI ga_win_thread_trampoline(LPVOID lpParam) {
    ga_win_thread_arg_t *warg = (ga_win_thread_arg_t*)lpParam;
    ga_thread_func fn = warg->func;
    void *arg = warg->arg;
    free(warg);
    fn(arg);
    return 0;
}

static inline int ga_thread_create(ga_thread_t *thread, void *(*start_routine)(void *), void *arg) {
    ga_win_thread_arg_t *warg = (ga_win_thread_arg_t*)malloc(sizeof(ga_win_thread_arg_t));
    warg->func = start_routine;
    warg->arg = arg;
    *thread = CreateThread(NULL, 0, ga_win_thread_trampoline, warg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
}

static inline int ga_thread_join(ga_thread_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}
#endif

// Fast PRNG Helpers
static inline uint32_t ga_xorshift32(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0x85431249U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float ga_rand_float(uint32_t *rng) {
    return (float)(ga_xorshift32(rng) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static inline float ga_rand_range(float min_v, float max_v, uint32_t *rng) {
    return min_v + ga_rand_float(rng) * (max_v - min_v);
}

static inline int ga_rand_int_range(int min_v, int max_v, uint32_t *rng) {
    if (max_v <= min_v) return min_v;
    return min_v + (int)(ga_xorshift32(rng) % (uint32_t)(max_v - min_v + 1));
}

static void ga_ensure_parent_dir(const char *filepath) {
    if (!filepath || !*filepath) return;
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s", filepath);
    char *last_slash = strrchr(path_buf, '/');
    if (!last_slash) last_slash = strrchr(path_buf, '\\');
    if (last_slash) {
        *last_slash = '\0';
        #ifdef _WIN32
        mkdir(path_buf);
        #else
        mkdir(path_buf, 0755);
        #endif
    }
}

// Config & Chromosome Initialization
void ga_config_init_default(GAConfig *cfg, EngineType target_engine) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(GAConfig));
    cfg->target_engine = (target_engine == ENGINE_TYPE_MCTS_UCB1) ? ENGINE_TYPE_MCTS_UCB1 : ENGINE_TYPE_MCTS_PUCT;
    cfg->population_size = GA_DEFAULT_POPULATION;
    cfg->generations = GA_DEFAULT_GENERATIONS;
    cfg->games_per_pair = GA_DEFAULT_GAMES_PER_PAIR;
    cfg->time_budget = GA_DEFAULT_TIME_BUDGET;
    cfg->max_plies = 80;
    cfg->opening_plies = 2;
    cfg->mutation_rate = GA_DEFAULT_MUTATION_RATE;
    cfg->mutation_scale = GA_DEFAULT_MUTATION_SCALE;
    cfg->crossover_rate = GA_DEFAULT_CROSSOVER_RATE;
    cfg->elite_count = GA_DEFAULT_ELITE_COUNT;
    cfg->threads = 4;
    cfg->csv_path[0] = '\0';
    cfg->quiet = false;
    cfg->verbose = false;
    cfg->seed = (uint32_t)time(NULL);
}

void ga_chromosome_sanitize(Chromosome *c) {
    if (!c) return;

    if (c->target_engine == ENGINE_TYPE_MCTS_UCB1) {
        // Zero unused PUCT genome parameters
        if (c->ucb1.book_mode == BOOK_MODE_OFF || c->ucb1.book_mode == BOOK_MODE_BEST) {
            c->ucb1.book_temperature = 0.0f;
        }
    } else {
        // PUCT genome sanitization
        if (c->puct.use_guided_book) {
            c->puct.book_mode = BOOK_MODE_OFF;
            c->puct.book_temperature = 0.0f;
            if (c->puct.lambda_book < 0.0f) c->puct.lambda_book = 0.0f;
            if (c->puct.lambda_book > 1.0f) c->puct.lambda_book = 1.0f;
        } else {
            c->puct.lambda_book = 0.0f;
            if (c->puct.book_mode == BOOK_MODE_OFF || c->puct.book_mode == BOOK_MODE_BEST) {
                c->puct.book_temperature = 0.0f;
            }
        }
    }
}

void ga_chromosome_init_default(Chromosome *c, EngineType target_engine) {
    if (!c) return;
    memset(c, 0, sizeof(Chromosome));
    c->target_engine = target_engine;
    
    // Common baseline defaults
    c->rollout_epsilon = 0.15f;
    c->max_rollout_depth = 70;
    c->use_db = true;

    if (target_engine == ENGINE_TYPE_MCTS_UCB1) {
        c->ucb1.exploration_alpha = 0.6422f;
        c->ucb1.book_mode = BOOK_MODE_GOOD;
        c->ucb1.book_temperature = 0.0500f;
        c->max_rollout_depth = 51;
        c->rollout_epsilon = 0.3128f;
        c->use_db = false;
    } else {
        c->puct.c_puct = 2.5857f;
        c->puct.puct_temperature = 2.2048f;
        c->puct.use_guided_book = true;
        c->puct.lambda_book = 0.0791f;
        c->puct.book_mode = BOOK_MODE_OFF;
        c->puct.book_temperature = 0.0f;
        c->max_rollout_depth = 112;
        c->rollout_epsilon = 0.3416f;
        c->use_db = false;
    }

    ga_chromosome_sanitize(c);
}

void ga_chromosome_randomize(Chromosome *c, EngineType target_engine, uint32_t *rng) {
    if (!c) return;
    memset(c, 0, sizeof(Chromosome));
    c->target_engine = target_engine;
    
    // Common genes
    c->rollout_epsilon = ga_rand_range(0.02f, 0.45f, rng);
    c->max_rollout_depth = ga_rand_int_range(20, 150, rng);
    c->use_db = (ga_rand_float(rng) > 0.05f);   // 95% chance true

    if (target_engine == ENGINE_TYPE_MCTS_UCB1) {
        c->ucb1.exploration_alpha = ga_rand_range(0.2f, 3.0f, rng);
        int bm = ga_rand_int_range(0, 3, rng);
        c->ucb1.book_mode = (BookPlayMode)bm;
        if (c->ucb1.book_mode == BOOK_MODE_GOOD || c->ucb1.book_mode == BOOK_MODE_ALL) {
            c->ucb1.book_temperature = ga_rand_range(0.1f, 2.5f, rng);
        } else {
            c->ucb1.book_temperature = 0.0f;
        }
    } else {
        c->puct.c_puct = ga_rand_range(0.5f, 4.0f, rng);
        c->puct.puct_temperature = ga_rand_range(0.1f, 2.5f, rng);
        c->puct.use_guided_book = (ga_rand_float(rng) > 0.30f); // 70% chance guided
        if (c->puct.use_guided_book) {
            c->puct.lambda_book = ga_rand_range(0.10f, 0.95f, rng);
            c->puct.book_mode = BOOK_MODE_OFF;
            c->puct.book_temperature = 0.0f;
        } else {
            c->puct.lambda_book = 0.0f;
            int bm = ga_rand_int_range(0, 3, rng);
            c->puct.book_mode = (BookPlayMode)bm;
            if (c->puct.book_mode == BOOK_MODE_GOOD || c->puct.book_mode == BOOK_MODE_ALL) {
                c->puct.book_temperature = ga_rand_range(0.1f, 2.5f, rng);
            } else {
                c->puct.book_temperature = 0.0f;
            }
        }
    }
    
    ga_chromosome_clamp(c);
}

void ga_chromosome_clamp(Chromosome *c) {
    if (!c) return;
    
    // Common genes clamp
    if (c->rollout_epsilon < 0.01f) c->rollout_epsilon = 0.01f;
    if (c->rollout_epsilon > 0.50f) c->rollout_epsilon = 0.50f;
    
    if (c->max_rollout_depth < 10) c->max_rollout_depth = 10;
    if (c->max_rollout_depth > 150) c->max_rollout_depth = 150;

    if (c->target_engine == ENGINE_TYPE_MCTS_UCB1) {
        if (c->ucb1.exploration_alpha < 0.1f) c->ucb1.exploration_alpha = 0.1f;
        if (c->ucb1.exploration_alpha > 4.0f) c->ucb1.exploration_alpha = 4.0f;
        
        if (c->ucb1.book_temperature < 0.05f) c->ucb1.book_temperature = 0.05f;
        if (c->ucb1.book_temperature > 4.0f) c->ucb1.book_temperature = 4.0f;
    } else {
        if (c->puct.c_puct < 0.2f) c->puct.c_puct = 0.2f;
        if (c->puct.c_puct > 5.0f) c->puct.c_puct = 5.0f;
        
        if (c->puct.puct_temperature < 0.05f) c->puct.puct_temperature = 0.05f;
        if (c->puct.puct_temperature > 3.0f) c->puct.puct_temperature = 3.0f;
        
        if (c->puct.lambda_book < 0.0f) c->puct.lambda_book = 0.0f;
        if (c->puct.lambda_book > 1.0f) c->puct.lambda_book = 1.0f;
        
        if (c->puct.book_temperature < 0.05f) c->puct.book_temperature = 0.05f;
        if (c->puct.book_temperature > 4.0f) c->puct.book_temperature = 4.0f;
    }

    ga_chromosome_sanitize(c);
}

void ga_chromosome_apply_to_config(const Chromosome *c, EngineConfig *cfg) {
    if (!c || !cfg) return;
    
    if (c->target_engine == ENGINE_TYPE_MCTS_PUCT) {
        cfg->puct_c_puct = c->puct.c_puct;
        cfg->puct_temperature = c->puct.puct_temperature;
        cfg->puct_rollout_epsilon = c->rollout_epsilon;
        cfg->puct_max_rollout_depth = c->max_rollout_depth;
        cfg->puct_use_db = c->use_db;
        cfg->puct_use_guided_book = c->puct.use_guided_book;
        cfg->puct_lambda_book = c->puct.lambda_book;
        cfg->puct_use_book = (!c->puct.use_guided_book) && (c->puct.book_mode != BOOK_MODE_OFF);
        cfg->book_mode = c->puct.use_guided_book ? BOOK_MODE_OFF : c->puct.book_mode;
        cfg->book_temperature = c->puct.book_temperature;
    } else {
        cfg->mcts_exploration = c->ucb1.exploration_alpha;
        cfg->mcts_rollout_epsilon = c->rollout_epsilon;
        cfg->mcts_max_rollout_depth = c->max_rollout_depth;
        cfg->mcts_use_db = c->use_db;
        cfg->mcts_use_book = (c->ucb1.book_mode != BOOK_MODE_OFF);
        cfg->book_mode = c->ucb1.book_mode;
        cfg->book_temperature = c->ucb1.book_temperature;
    }
}

// Crossover & Mutation Operators
void ga_chromosome_crossover(const Chromosome *p1, const Chromosome *p2, Chromosome *child1, Chromosome *child2, float crossover_rate, uint32_t *rng) {
    if (!p1 || !p2 || !child1 || !child2) return;
    
    *child1 = *p1;
    *child2 = *p2;
    
    child1->games_played = child1->wins = child1->draws = child1->losses = 0;
    child1->points = child1->score_pct = child1->elo = child1->fitness = 0.0;
    child1->total_time_spent = 0.0; child1->total_moves = 0;
    
    child2->games_played = child2->wins = child2->draws = child2->losses = 0;
    child2->points = child2->score_pct = child2->elo = child2->fitness = 0.0;
    child2->total_time_spent = 0.0; child2->total_moves = 0;

    if (ga_rand_float(rng) > crossover_rate) {
        return; // No crossover, cloned
    }

    // Common continuous genes arithmetic blend
    float gamma = ga_rand_range(-0.1f, 1.1f, rng);
    child1->rollout_epsilon = gamma * p1->rollout_epsilon + (1.0f - gamma) * p2->rollout_epsilon;
    child2->rollout_epsilon = (1.0f - gamma) * p1->rollout_epsilon + gamma * p2->rollout_epsilon;

    float delta = ga_rand_range(-0.1f, 1.1f, rng);
    child1->max_rollout_depth = (int)(delta * (float)p1->max_rollout_depth + (1.0f - delta) * (float)p2->max_rollout_depth + 0.5f);
    child2->max_rollout_depth = (int)((1.0f - delta) * (float)p1->max_rollout_depth + delta * (float)p2->max_rollout_depth + 0.5f);

    child1->use_db = (ga_rand_float(rng) < 0.5f) ? p1->use_db : p2->use_db;
    child2->use_db = (ga_rand_float(rng) < 0.5f) ? p2->use_db : p1->use_db;

    if (p1->target_engine == ENGINE_TYPE_MCTS_UCB1) {
        float alpha = ga_rand_range(-0.1f, 1.1f, rng);
        child1->ucb1.exploration_alpha = alpha * p1->ucb1.exploration_alpha + (1.0f - alpha) * p2->ucb1.exploration_alpha;
        child2->ucb1.exploration_alpha = (1.0f - alpha) * p1->ucb1.exploration_alpha + alpha * p2->ucb1.exploration_alpha;

        child1->ucb1.book_mode = (ga_rand_float(rng) < 0.5f) ? p1->ucb1.book_mode : p2->ucb1.book_mode;
        child2->ucb1.book_mode = (ga_rand_float(rng) < 0.5f) ? p2->ucb1.book_mode : p1->ucb1.book_mode;

        float eta = ga_rand_range(-0.1f, 1.1f, rng);
        child1->ucb1.book_temperature = eta * p1->ucb1.book_temperature + (1.0f - eta) * p2->ucb1.book_temperature;
        child2->ucb1.book_temperature = (1.0f - eta) * p1->ucb1.book_temperature + eta * p2->ucb1.book_temperature;
    } else {
        float alpha = ga_rand_range(-0.1f, 1.1f, rng);
        float beta  = ga_rand_range(-0.1f, 1.1f, rng);

        child1->puct.c_puct = alpha * p1->puct.c_puct + (1.0f - alpha) * p2->puct.c_puct;
        child2->puct.c_puct = (1.0f - alpha) * p1->puct.c_puct + alpha * p2->puct.c_puct;
        
        child1->puct.puct_temperature = beta * p1->puct.puct_temperature + (1.0f - beta) * p2->puct.puct_temperature;
        child2->puct.puct_temperature = (1.0f - beta) * p1->puct.puct_temperature + beta * p2->puct.puct_temperature;

        child1->puct.use_guided_book = (ga_rand_float(rng) < 0.5f) ? p1->puct.use_guided_book : p2->puct.use_guided_book;
        child2->puct.use_guided_book = (ga_rand_float(rng) < 0.5f) ? p2->puct.use_guided_book : p1->puct.use_guided_book;

        float lam = ga_rand_range(-0.1f, 1.1f, rng);
        child1->puct.lambda_book = lam * p1->puct.lambda_book + (1.0f - lam) * p2->puct.lambda_book;
        child2->puct.lambda_book = (1.0f - lam) * p1->puct.lambda_book + lam * p2->puct.lambda_book;

        child1->puct.book_mode = (ga_rand_float(rng) < 0.5f) ? p1->puct.book_mode : p2->puct.book_mode;
        child2->puct.book_mode = (ga_rand_float(rng) < 0.5f) ? p2->puct.book_mode : p1->puct.book_mode;

        float eta = ga_rand_range(-0.1f, 1.1f, rng);
        child1->puct.book_temperature = eta * p1->puct.book_temperature + (1.0f - eta) * p2->puct.book_temperature;
        child2->puct.book_temperature = (1.0f - eta) * p1->puct.book_temperature + eta * p2->puct.book_temperature;
    }

    ga_chromosome_clamp(child1);
    ga_chromosome_clamp(child2);
}

void ga_chromosome_mutate(Chromosome *c, float mutation_rate, float mutation_scale, uint32_t *rng) {
    if (!c) return;

    // Common genes mutation
    if (ga_rand_float(rng) < mutation_rate) {
        c->rollout_epsilon += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 0.40f;
    }
    if (ga_rand_float(rng) < mutation_rate) {
        c->max_rollout_depth += (int)((ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 80.0f);
    }
    if (ga_rand_float(rng) < mutation_rate * 0.2f) {
        c->use_db = !c->use_db;
    }

    if (c->target_engine == ENGINE_TYPE_MCTS_UCB1) {
        if (ga_rand_float(rng) < mutation_rate) {
            c->ucb1.exploration_alpha += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 2.5f;
        }
        if (ga_rand_float(rng) < mutation_rate * 0.5f) {
            c->ucb1.book_mode = (BookPlayMode)ga_rand_int_range(0, 3, rng);
        }
        // Mutate book_temperature ONLY if book_mode is GOOD or ALL
        if (c->ucb1.book_mode == BOOK_MODE_GOOD || c->ucb1.book_mode == BOOK_MODE_ALL) {
            if (ga_rand_float(rng) < mutation_rate) {
                c->ucb1.book_temperature += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 2.0f;
            }
        }
    } else {
        if (ga_rand_float(rng) < mutation_rate) {
            c->puct.c_puct += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 3.0f;
        }
        if (ga_rand_float(rng) < mutation_rate) {
            c->puct.puct_temperature += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 2.0f;
        }
        if (ga_rand_float(rng) < mutation_rate * 0.3f) {
            c->puct.use_guided_book = !c->puct.use_guided_book;
        }
        if (c->puct.use_guided_book) {
            // Mutate lambda_book ONLY if guided blending is active
            if (ga_rand_float(rng) < mutation_rate) {
                c->puct.lambda_book += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 0.5f;
            }
        } else {
            // Mutate instant book parameters ONLY if guided blending is OFF
            if (ga_rand_float(rng) < mutation_rate * 0.5f) {
                c->puct.book_mode = (BookPlayMode)ga_rand_int_range(0, 3, rng);
            }
            if (c->puct.book_mode == BOOK_MODE_GOOD || c->puct.book_mode == BOOK_MODE_ALL) {
                if (ga_rand_float(rng) < mutation_rate) {
                    c->puct.book_temperature += (ga_rand_float(rng) * 2.0f - 1.0f) * mutation_scale * 2.0f;
                }
            }
        }
    }

    ga_chromosome_clamp(c);
}

// Population Lifecycle
void ga_population_init(Population *pop, const GAConfig *cfg, uint32_t *rng) {
    if (!pop || !cfg) return;
    memset(pop, 0, sizeof(Population));
    pop->size = cfg->population_size;
    pop->generation = 0;
    pop->best_idx = 0;

    // Seed individual 0 with default baseline
    ga_chromosome_init_default(&pop->individuals[0], cfg->target_engine);
    pop->individuals[0].id = 0;
    pop->individuals[0].generation = 0;

    // Randomize rest of population
    for (int i = 1; i < pop->size; i++) {
        ga_chromosome_randomize(&pop->individuals[i], cfg->target_engine, rng);
        pop->individuals[i].id = i;
        pop->individuals[i].generation = 0;
    }
}

// Single Game Match Execution Between Two Individuals
typedef struct {
    int white_idx;
    int black_idx;
    int game_idx;
    int opening_seed;
} MatchPairTask;

static void ga_apply_chromosome_to_engine(const Chromosome *chr, EngineType type, void *engine_state, double time_budget) {
    if (!chr || !engine_state) return;
    if (type == ENGINE_TYPE_MCTS_PUCT) {
        engine_mcts_puct_set_time_budget(engine_state, time_budget);
        engine_mcts_puct_set_c_puct(engine_state, chr->puct.c_puct);
        engine_mcts_puct_set_temperature(engine_state, chr->puct.puct_temperature);
        engine_mcts_puct_set_max_rollout_depth(engine_state, chr->max_rollout_depth);
        engine_mcts_puct_set_rollout_epsilon(engine_state, chr->rollout_epsilon);
        engine_mcts_puct_set_use_db(engine_state, chr->use_db);
        engine_mcts_puct_set_guided_book(engine_state, chr->puct.use_guided_book, chr->puct.lambda_book);
        engine_mcts_puct_set_use_book(engine_state, (!chr->puct.use_guided_book) && (chr->puct.book_mode != BOOK_MODE_OFF));
        engine_mcts_puct_set_book_mode(engine_state, chr->puct.use_guided_book ? BOOK_MODE_OFF : chr->puct.book_mode);
        engine_mcts_puct_set_book_temperature(engine_state, chr->puct.book_temperature);
        engine_mcts_puct_set_debug_log(engine_state, false);
    } else if (type == ENGINE_TYPE_MCTS_UCB1) {
        engine_mcts_ucb1_set_time_budget(engine_state, time_budget);
        engine_mcts_ucb1_set_exploration(engine_state, chr->ucb1.exploration_alpha);
        engine_mcts_ucb1_set_max_rollout_depth(engine_state, chr->max_rollout_depth);
        engine_mcts_ucb1_set_rollout_epsilon(engine_state, chr->rollout_epsilon);
        engine_mcts_ucb1_set_use_db(engine_state, chr->use_db);
        engine_mcts_ucb1_set_use_book(engine_state, chr->ucb1.book_mode != BOOK_MODE_OFF);
        engine_mcts_ucb1_set_book_mode(engine_state, chr->ucb1.book_mode);
        engine_mcts_ucb1_set_book_temperature(engine_state, chr->ucb1.book_temperature);
        engine_mcts_ucb1_set_debug_log(engine_state, false);
    }
}

static void ga_play_match_game(const GAConfig *cfg, Engine *white_eng, Engine *black_eng,
                               const Chromosome *white_chr, const Chromosome *black_chr, int opening_seed,
                               double *out_white_time, int *out_white_moves,
                               double *out_black_time, int *out_black_moves,
                               bool *out_is_draw, Player *out_winner) {
    GameState game;
    game_init(&game, MODE_CPUVSCPU, PLAYER_WHITE, cfg->target_engine, cfg->target_engine);

    engine_reset(white_eng, cfg->target_engine);
    engine_reset(black_eng, cfg->target_engine);

    ga_apply_chromosome_to_engine(white_chr, cfg->target_engine, white_eng->internal_state, cfg->time_budget);
    ga_apply_chromosome_to_engine(black_chr, cfg->target_engine, black_eng->internal_state, cfg->time_budget);

    // Opening Randomization to prevent deterministic playout loops
    uint32_t op_rng = (uint32_t)opening_seed ^ 0x9e3779b9U;
    for (int p = 0; p < cfg->opening_plies && !game.is_game_over; p++) {
        const MoveList *legal = game_get_valid_moves(&game);
        if (!legal || legal->count == 0) break;
        int r_idx = (int)(ga_xorshift32(&op_rng) % legal->count);
        game_execute_move(&game, legal->moves[r_idx]);
    }

    double w_time = 0.0, b_time = 0.0;
    int w_moves = 0, b_moves = 0;
    int ply_count = 0;

    while (!game.is_game_over && ply_count < cfg->max_plies) {
        Player cur = game.current_player;
        Engine *eng = (cur == PLAYER_WHITE) ? white_eng : black_eng;

        #ifdef _WIN32
        LARGE_INTEGER fq, c1, c2;
        QueryPerformanceFrequency(&fq);
        QueryPerformanceCounter(&c1);
        Move mv = eng->get_move(eng->internal_state, &game);
        QueryPerformanceCounter(&c2);
        double elapsed = (double)(c2.QuadPart - c1.QuadPart) / (double)fq.QuadPart;
        #else
        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        Move mv = eng->get_move(eng->internal_state, &game);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        double elapsed = (double)(t2.tv_sec - t1.tv_sec) + (double)(t2.tv_nsec - t1.tv_nsec) * 1e-9;
        #endif

        if (cur == PLAYER_WHITE) {
            w_time += elapsed;
            w_moves++;
        } else {
            b_time += elapsed;
            b_moves++;
        }

        if (move_is_none(mv)) {
            break;
        }

        game_execute_move(&game, mv);
        ply_count++;
    }

    *out_white_time = w_time;
    *out_white_moves = w_moves;
    *out_black_time = b_time;
    *out_black_moves = b_moves;

    if (game.is_game_over) {
        *out_is_draw = game.is_draw;
        *out_winner = game.winner;
    } else {
        // Exceeded max plies -> Draw
        *out_is_draw = true;
        *out_winner = PLAYER_WHITE;
    }
}

// Multi-threaded Tournament Evaluator
typedef struct {
    const GAConfig *cfg;
    Population     *pop;
    MatchPairTask  *tasks;
    int            task_count;
    int            next_task_idx;
    ga_mutex_t     mutex;
    int            completed_tasks;
    int            total_games;
} GAEvalWorkerContext;

static void* ga_eval_worker_thread(void *arg) {
    GAEvalWorkerContext *ctx = (GAEvalWorkerContext*)arg;

    Engine white_eng = engine_create(ctx->cfg->target_engine);
    Engine black_eng = engine_create(ctx->cfg->target_engine);

    while (1) {
        int t_idx = -1;
        ga_mutex_lock(&ctx->mutex);
        if (ctx->next_task_idx < ctx->task_count) {
            t_idx = ctx->next_task_idx++;
        }
        ga_mutex_unlock(&ctx->mutex);

        if (t_idx < 0) break; // All tasks processed

        MatchPairTask task = ctx->tasks[t_idx];
        const Chromosome *w_chr = &ctx->pop->individuals[task.white_idx];
        const Chromosome *b_chr = &ctx->pop->individuals[task.black_idx];

        double w_time = 0.0, b_time = 0.0;
        int w_moves = 0, b_moves = 0;
        bool is_draw = false;
        Player winner = PLAYER_WHITE;

        ga_play_match_game(ctx->cfg, &white_eng, &black_eng,
                           w_chr, b_chr, task.opening_seed,
                           &w_time, &w_moves, &b_time, &b_moves,
                           &is_draw, &winner);

        // Accumulate statistics safely under lock
        ga_mutex_lock(&ctx->mutex);
        Chromosome *w_ind = &ctx->pop->individuals[task.white_idx];
        Chromosome *b_ind = &ctx->pop->individuals[task.black_idx];

        w_ind->games_played++;
        b_ind->games_played++;
        w_ind->total_time_spent += w_time;
        b_ind->total_time_spent += b_time;
        w_ind->total_moves += w_moves;
        b_ind->total_moves += b_moves;

        if (is_draw) {
            w_ind->draws++;
            b_ind->draws++;
            w_ind->points += 0.5;
            b_ind->points += 0.5;
        } else if (winner == PLAYER_WHITE) {
            w_ind->wins++;
            b_ind->losses++;
            w_ind->points += 1.0;
        } else if (winner == PLAYER_BLACK) {
            b_ind->wins++;
            w_ind->losses++;
            b_ind->points += 1.0;
        } else {
            w_ind->draws++;
            b_ind->draws++;
            w_ind->points += 0.5;
            b_ind->points += 0.5;
        }

        ctx->completed_tasks++;
        if (!ctx->cfg->quiet && ctx->completed_tasks % 10 == 0) {
            fprintf(stderr, "\r  -> Gen %d Evaluation: [%d / %d games] (%.1f%%)...",
                    ctx->pop->generation + 1, ctx->completed_tasks, ctx->task_count,
                    ((double)ctx->completed_tasks / (double)ctx->task_count) * 100.0);
            fflush(stderr);
        }
        ga_mutex_unlock(&ctx->mutex);
    }

    engine_destroy(&white_eng);
    engine_destroy(&black_eng);
    return NULL;
}

// Comparator for sorting population by composite fitness descending
static int ga_chromosome_cmp(const void *a, const void *b) {
    const Chromosome *c1 = (const Chromosome*)a;
    const Chromosome *c2 = (const Chromosome*)b;
    if (c2->fitness > c1->fitness) return 1;
    if (c2->fitness < c1->fitness) return -1;
    if (c2->points > c1->points) return 1;
    if (c2->points < c1->points) return -1;
    if (c2->wins > c1->wins) return 1;
    if (c2->wins < c1->wins) return -1;
    return 0;
}

void ga_population_evaluate(Population *pop, const GAConfig *cfg) {
    if (!pop || !cfg || pop->size <= 1) return;

    // Reset scores for this generation
    for (int i = 0; i < pop->size; i++) {
        pop->individuals[i].generation = pop->generation;
        pop->individuals[i].games_played = 0;
        pop->individuals[i].wins = 0;
        pop->individuals[i].draws = 0;
        pop->individuals[i].losses = 0;
        pop->individuals[i].points = 0.0;
        pop->individuals[i].score_pct = 0.0;
        pop->individuals[i].elo = 1500.0;
        pop->individuals[i].total_time_spent = 0.0;
        pop->individuals[i].total_moves = 0;
        pop->individuals[i].fitness = 0.0;
    }

    // Build Round-Robin match tasks
    int total_pairs = (pop->size * (pop->size - 1)) / 2;
    int total_tasks = total_pairs * cfg->games_per_pair;
    MatchPairTask *tasks = (MatchPairTask*)malloc(sizeof(MatchPairTask) * total_tasks);
    if (!tasks) return;

    int task_idx = 0;
    for (int i = 0; i < pop->size; i++) {
        for (int j = i + 1; j < pop->size; j++) {
            for (int g = 0; g < cfg->games_per_pair; g++) {
                MatchPairTask task;
                // Alternate colors each game in the pair
                if (g % 2 == 0) {
                    task.white_idx = i;
                    task.black_idx = j;
                } else {
                    task.white_idx = j;
                    task.black_idx = i;
                }
                task.game_idx = g;
                task.opening_seed = (pop->generation * 10007) + (i * 101) + (j * 17) + g;
                tasks[task_idx++] = task;
            }
        }
    }

    GAEvalWorkerContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;
    ctx.pop = pop;
    ctx.tasks = tasks;
    ctx.task_count = total_tasks;
    ctx.next_task_idx = 0;
    ctx.completed_tasks = 0;
    ga_mutex_init(&ctx.mutex);

    int num_threads = cfg->threads > 0 ? cfg->threads : 1;
    if (num_threads > 16) num_threads = 16;
    if (num_threads > total_tasks) num_threads = total_tasks;

    if (num_threads > 1) {
        ga_thread_t *threads = (ga_thread_t*)malloc(sizeof(ga_thread_t) * num_threads);

        for (int t = 0; t < num_threads; t++) {
            ga_thread_create(&threads[t], ga_eval_worker_thread, &ctx);
        }

        for (int t = 0; t < num_threads; t++) {
            ga_thread_join(threads[t]);
        }
        free(threads);
    } else {
        ga_eval_worker_thread(&ctx);
    }

    if (!cfg->quiet) {
        fprintf(stderr, "\r  -> Gen %d Evaluation: [%d / %d games] (100.0%%) Done.\n",
                pop->generation + 1, total_tasks, total_tasks);
        fflush(stderr);
    }

    free(tasks);
    ga_mutex_destroy(&ctx.mutex);

    // Compute composite fitness & Bayes-Elo for each individual
    for (int i = 0; i < pop->size; i++) {
        Chromosome *c = &pop->individuals[i];
        if (c->games_played > 0) {
            c->score_pct = (c->points / (double)c->games_played) * 100.0;
            
            // Bayes-Elo approximation relative to 1500 baseline
            double s_clamped = c->score_pct;
            if (s_clamped < 1.0) s_clamped = 1.0;
            if (s_clamped > 99.0) s_clamped = 99.0;
            c->elo = 1500.0 + 400.0 * log10(s_clamped / (100.0 - s_clamped));

            // Composite fitness: Points * 100 + Win bonus - Speed penalty
            double avg_move_time = (c->total_moves > 0) ? (c->total_time_spent / c->total_moves) : 0.0;
            c->fitness = (c->score_pct * 10.0) + ((double)c->wins * 1.5) - (avg_move_time * 5.0);
        } else {
            c->fitness = 0.0;
        }
    }

    // Sort population descending by fitness
    qsort(pop->individuals, pop->size, sizeof(Chromosome), ga_chromosome_cmp);
    pop->best_idx = 0;
}

// Tournament selection of size k=2
static int ga_select_parent(const Population *pop, uint32_t *rng) {
    int i1 = (int)(ga_xorshift32(rng) % (uint32_t)pop->size);
    int i2 = (int)(ga_xorshift32(rng) % (uint32_t)pop->size);
    return (pop->individuals[i1].fitness >= pop->individuals[i2].fitness) ? i1 : i2;
}

void ga_population_evolve(Population *pop, const GAConfig *cfg, uint32_t *rng) {
    if (!pop || !cfg || pop->size <= 1) return;

    Chromosome next_gen[GA_MAX_POPULATION];
    int next_count = 0;

    // 1. Elitism: preserve top elite individuals
    int elites = cfg->elite_count;
    if (elites > pop->size) elites = pop->size;
    for (int e = 0; e < elites; e++) {
        next_gen[next_count] = pop->individuals[e];
        next_gen[next_count].id = next_count;
        next_gen[next_count].generation = pop->generation + 1;
        next_count++;
    }

    // 2. Generate remaining offspring via Crossover and Mutation
    while (next_count < pop->size) {
        int p1_idx = ga_select_parent(pop, rng);
        int p2_idx = ga_select_parent(pop, rng);

        Chromosome c1, c2;
        ga_chromosome_crossover(&pop->individuals[p1_idx], &pop->individuals[p2_idx],
                                &c1, &c2, cfg->crossover_rate, rng);

        ga_chromosome_mutate(&c1, cfg->mutation_rate, cfg->mutation_scale, rng);
        ga_chromosome_mutate(&c2, cfg->mutation_rate, cfg->mutation_scale, rng);

        c1.id = next_count;
        c1.generation = pop->generation + 1;
        next_gen[next_count++] = c1;

        if (next_count < pop->size) {
            c2.id = next_count;
            c2.generation = pop->generation + 1;
            next_gen[next_count++] = c2;
        }
    }

    // Copy next generation into population and zero all generation match statistics
    for (int i = 0; i < pop->size; i++) {
        pop->individuals[i] = next_gen[i];
        pop->individuals[i].games_played = 0;
        pop->individuals[i].wins = 0;
        pop->individuals[i].draws = 0;
        pop->individuals[i].losses = 0;
        pop->individuals[i].points = 0.0;
        pop->individuals[i].score_pct = 0.0;
        pop->individuals[i].elo = 1500.0;
        pop->individuals[i].fitness = 0.0;
        pop->individuals[i].total_time_spent = 0.0;
        pop->individuals[i].total_moves = 0;
    }
    pop->generation++;
}

static void ga_print_generation_summary(const Population *pop, const GAConfig *cfg) {
    printf("\n+-----------------------------------------------------------------------------------------------------+\n");
    printf("| Gen %-2d Leaderboard (Target: %-9s | Pop: %-2d | Time: %.2fs)                                      |\n",
           pop->generation + 1, engine_get_type_name(cfg->target_engine), pop->size, cfg->time_budget);
    printf("+------+-------+---------+--------+--------+-------+--------------------------------------------------+\n");
    if (cfg->target_engine == ENGINE_TYPE_MCTS_PUCT) {
        printf("| Rank | Id    | Score %% | Points | W/D/L  | Elo   | c_puct | tau  | eps_roll | depth | guided | lambda/book | db  |\n");
    } else {
        printf("| Rank | Id    | Score %% | Points | W/D/L  | Elo   | alpha  | eps_roll | depth | book_mode | book_tau | db  |\n");
    }
    printf("+------+-------+---------+--------+--------+-------+--------------------------------------------------+\n");

    int display_count = pop->size > 8 ? 8 : pop->size;
    for (int r = 0; r < display_count; r++) {
        const Chromosome *c = &pop->individuals[r];
        if (cfg->target_engine == ENGINE_TYPE_MCTS_PUCT) {
            char book_info[24];
            if (c->puct.use_guided_book) {
                snprintf(book_info, sizeof(book_info), "lam=%.2f", c->puct.lambda_book);
            } else {
                const char *bm_str = (c->puct.book_mode == BOOK_MODE_BEST) ? "BEST" :
                                     (c->puct.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                                     (c->puct.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";
                snprintf(book_info, sizeof(book_info), "%s (t=%.1f)", bm_str, c->puct.book_temperature);
            }
            printf("| #%-3d | #%-4d | %5.1f%%  | %5.1f  | %2d/%2d/%-2d | %5.0f | %6.2f | %4.2f| %8.2f | %5d | %-6s | %-11s | %-3s |\n",
                   r + 1, c->id, c->score_pct, c->points, c->wins, c->draws, c->losses, c->elo,
                   c->puct.c_puct, c->puct.puct_temperature, c->rollout_epsilon, c->max_rollout_depth,
                   c->puct.use_guided_book ? "ON" : "OFF", book_info, c->use_db ? "ON" : "OFF");
        } else {
            const char *bm_str = (c->ucb1.book_mode == BOOK_MODE_BEST) ? "BEST" :
                                 (c->ucb1.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                                 (c->ucb1.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";
            printf("| #%-3d | #%-4d | %5.1f%%  | %5.1f  | %2d/%2d/%-2d | %5.0f | %6.2f | %8.2f | %5d | %-9s | %8.2f | %-3s |\n",
                   r + 1, c->id, c->score_pct, c->points, c->wins, c->draws, c->losses, c->elo,
                   c->ucb1.exploration_alpha, c->rollout_epsilon, c->max_rollout_depth,
                   bm_str, c->ucb1.book_temperature, c->use_db ? "ON" : "OFF");
        }
    }
    printf("+------+-------+---------+--------+--------+-------+--------------------------------------------------+\n");
}

static void ga_export_csv_row(FILE *f, const Chromosome *c, int rank, const GAConfig *cfg) {
    if (!f || !c) return;

    if (cfg->target_engine == ENGINE_TYPE_MCTS_UCB1) {
        const char *bm_str = (c->ucb1.book_mode == BOOK_MODE_BEST) ? "BEST" :
                             (c->ucb1.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                             (c->ucb1.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";
        fprintf(f, "%d,%d,%d,\"%s\",%.2f,%.1f,%d,%d,%d,%d,%.1f,%.4f,%.4f,%.4f,%d,\"%s\",%.4f,%d\n",
                c->generation + 1, rank, c->id, engine_get_type_name(cfg->target_engine),
                c->score_pct, c->points, c->games_played, c->wins, c->draws, c->losses, c->elo,
                c->ucb1.exploration_alpha, c->rollout_epsilon, cfg->time_budget, c->max_rollout_depth,
                bm_str, c->ucb1.book_temperature, c->use_db ? 1 : 0);
    } else {
        const char *bm_str = (c->puct.book_mode == BOOK_MODE_BEST) ? "BEST" :
                             (c->puct.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                             (c->puct.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";
        fprintf(f, "%d,%d,%d,\"%s\",%.2f,%.1f,%d,%d,%d,%d,%.1f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.4f,\"%s\",%.4f,%d\n",
                c->generation + 1, rank, c->id, engine_get_type_name(cfg->target_engine),
                c->score_pct, c->points, c->games_played, c->wins, c->draws, c->losses, c->elo,
                c->puct.c_puct, c->puct.puct_temperature, c->rollout_epsilon, cfg->time_budget, c->max_rollout_depth,
                c->puct.use_guided_book ? 1 : 0, c->puct.lambda_book, bm_str, c->puct.book_temperature, c->use_db ? 1 : 0);
    }
}

typedef struct {
    uint32_t   magic; // 0x4741434BU "GACK"
    int        target_engine;
    int        pop_size;
    int        completed_gen;
    int        total_games;
    uint32_t   rng;
    Chromosome best_overall;
    Population pop;
} GACheckpoint;

static bool ga_save_checkpoint(const char *csv_path, int completed_gen, int total_games, uint32_t rng, const Chromosome *best, const Population *pop, int target_engine) {
    if (!csv_path || !*csv_path) return false;
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s.ckpt", csv_path);
    FILE *f = fopen(ckpt_path, "wb");
    if (!f) return false;
    GACheckpoint ckpt;
    memset(&ckpt, 0, sizeof(ckpt));
    ckpt.magic = 0x4741434BU;
    ckpt.target_engine = target_engine;
    ckpt.pop_size = pop->size;
    ckpt.completed_gen = completed_gen;
    ckpt.total_games = total_games;
    ckpt.rng = rng;
    ckpt.best_overall = *best;
    ckpt.pop = *pop;
    fwrite(&ckpt, sizeof(ckpt), 1, f);
    fclose(f);
    return true;
}

static bool ga_load_checkpoint(const char *csv_path, int *out_completed_gen, int *out_total_games, uint32_t *out_rng, Chromosome *out_best, Population *out_pop, int target_engine, int expected_pop_size) {
    if (!csv_path || !*csv_path) return false;
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s.ckpt", csv_path);
    FILE *f = fopen(ckpt_path, "rb");
    if (!f) return false;
    GACheckpoint ckpt;
    if (fread(&ckpt, sizeof(ckpt), 1, f) != 1) {
        fclose(f);
        return false;
    }
    fclose(f);
    if (ckpt.magic != 0x4741434BU || ckpt.target_engine != target_engine || ckpt.pop_size != expected_pop_size) {
        return false;
    }
    *out_completed_gen = ckpt.completed_gen;
    *out_total_games = ckpt.total_games;
    *out_rng = ckpt.rng;
    *out_best = ckpt.best_overall;
    *out_pop = ckpt.pop;
    return true;
}

static void ga_remove_checkpoint(const char *csv_path) {
    if (!csv_path || !*csv_path) return;
    char ckpt_path[512];
    snprintf(ckpt_path, sizeof(ckpt_path), "%s.ckpt", csv_path);
    remove(ckpt_path);
}

// Main High-level Genetic Algorithm Tuning Runner
int tune_ga_run(const GAConfig *cfg, GAResult *out_result) {
    if (!cfg) return 1;

    printf("==============================================================================\n");
    printf("  Damascus - Automated Hyperparameter Tuning Engine (Genetic Algorithm)\n");
    printf("==============================================================================\n\n");
    printf("  -> Target Engine   : %s\n", engine_get_type_name(cfg->target_engine));
    printf("  -> Population Size : %d individuals\n", cfg->population_size);
    printf("  -> Generations     : %d\n", cfg->generations);
    printf("  -> Games per Pair  : %d (alternating colors)\n", cfg->games_per_pair);
    printf("  -> Time Budget     : %.2fs per move\n", cfg->time_budget);
    printf("  -> Mutation Rate   : %.2f (scale: %.2f)\n", cfg->mutation_rate, cfg->mutation_scale);
    printf("  -> Crossover Rate  : %.2f\n", cfg->crossover_rate);
    printf("  -> Elite Count     : %d\n", cfg->elite_count);
    printf("  -> Worker Threads  : %d\n", cfg->threads);
    if (cfg->csv_path[0] != '\0') {
        printf("  -> CSV Output      : %s\n", cfg->csv_path);
    }
    printf("\n");

    // Initialize WLD & Book Subsystems
    wld_init_backend(WLD_BACKEND_OFFICIAL_8PIECE);
    opening_book_init(BOOK_BACKEND_KINGSROW_ODB, NULL);

    uint32_t rng = cfg->seed ? cfg->seed : 0x12345678U;
    Population pop;
    Chromosome best_overall;
    memset(&best_overall, 0, sizeof(Chromosome));
    best_overall.fitness = -1e9;

    int total_games = 0;
    int start_gen = 0;
    bool resumed = false;

    if (cfg->csv_path[0] != '\0') {
        resumed = ga_load_checkpoint(cfg->csv_path, &start_gen, &total_games, &rng, &best_overall, &pop, cfg->target_engine, cfg->population_size);
    }

    FILE *csv_file = NULL;
    if (cfg->csv_path[0] != '\0') {
        ga_ensure_parent_dir(cfg->csv_path);
        if (resumed && start_gen > 0) {
            printf("  -> [AUTO-RESUME] Found checkpoint. Resuming from Generation %d (%d games already simulated)...\n\n",
                   start_gen + 1, total_games);
            csv_file = fopen(cfg->csv_path, "a");
        } else {
            csv_file = fopen(cfg->csv_path, "w");
            if (csv_file) {
                if (cfg->target_engine == ENGINE_TYPE_MCTS_UCB1) {
                    fprintf(csv_file, "generation,rank,id,target_engine,score_pct,points,games_played,wins,draws,losses,elo,exploration_alpha,rollout_epsilon,time_budget,max_rollout_depth,book_mode,book_temperature,use_db\n");
                } else {
                    fprintf(csv_file, "generation,rank,id,target_engine,score_pct,points,games_played,wins,draws,losses,elo,c_puct,puct_temperature,rollout_epsilon,time_budget,max_rollout_depth,use_guided_book,lambda_book,book_mode,book_temperature,use_db\n");
                }
            }
            ga_population_init(&pop, cfg, &rng);
            start_gen = 0;
            total_games = 0;
        }
    } else {
        ga_population_init(&pop, cfg, &rng);
    }

    #ifdef _WIN32
    LARGE_INTEGER fq, t_start, t_end;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t_start);
    #else
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    #endif

    int games_per_gen = ((cfg->population_size * (cfg->population_size - 1)) / 2) * cfg->games_per_pair;

    for (int gen = start_gen; gen < cfg->generations; gen++) {
        printf("--- Running Generation %d / %d ---\n", gen + 1, cfg->generations);
        ga_population_evaluate(&pop, cfg);
        total_games += games_per_gen;

        ga_print_generation_summary(&pop, cfg);

        // Update overall best
        if (pop.individuals[0].fitness > best_overall.fitness) {
            best_overall = pop.individuals[0];
        }

        // Export to CSV
        if (csv_file) {
            for (int r = 0; r < pop.size; r++) {
                ga_export_csv_row(csv_file, &pop.individuals[r], r + 1, cfg);
            }
            fflush(csv_file);
        }

        // Evolve to next generation if not at final step
        if (gen < cfg->generations - 1) {
            ga_population_evolve(&pop, cfg, &rng);
        }

        // Persist checkpoint after each generation
        if (cfg->csv_path[0] != '\0') {
            ga_save_checkpoint(cfg->csv_path, gen + 1, total_games, rng, &best_overall, &pop, cfg->target_engine);
        }
    }

    #ifdef _WIN32
    QueryPerformanceCounter(&t_end);
    double elapsed_sec = (double)(t_end.QuadPart - t_start.QuadPart) / (double)fq.QuadPart;
    #else
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed_sec = (double)(t_end.tv_sec - t_start.tv_sec) + (double)(t_end.tv_nsec - t_start.tv_nsec) * 1e-9;
    #endif

    if (cfg->csv_path[0] != '\0') {
        ga_remove_checkpoint(cfg->csv_path);
    }

    if (csv_file) {
        fclose(csv_file);
        printf("\nFull tuning history exported to CSV: %s\n", cfg->csv_path);
    }

    // Print Final Tuning Recommendations
    printf("\n==============================================================================\n");
    printf("  Genetic Algorithm Tuning Completed Successfully in %.2fs (%d Total Games)\n", elapsed_sec, total_games);
    printf("==============================================================================\n\n");
    printf("  >>> Optimal Discovered Hyperparameters for %s <<<\n\n", engine_get_type_name(cfg->target_engine));
    
    if (cfg->target_engine == ENGINE_TYPE_MCTS_PUCT) {
        const char *bm_str = (best_overall.puct.book_mode == BOOK_MODE_BEST) ? "BEST" :
                             (best_overall.puct.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                             (best_overall.puct.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";

        printf("  - c_puct (exploration)    : %.4f\n", best_overall.puct.c_puct);
        printf("  - tau (temperature)       : %.4f\n", best_overall.puct.puct_temperature);
        printf("  - rollout_epsilon         : %.4f\n", best_overall.rollout_epsilon);
        printf("  - max_rollout_depth       : %d\n", best_overall.max_rollout_depth);
        printf("  - use_guided_book         : %s\n", best_overall.puct.use_guided_book ? "true" : "false");
        if (best_overall.puct.use_guided_book) {
            printf("  - lambda_book (blending)  : %.4f\n", best_overall.puct.lambda_book);
        } else {
            printf("  - book_mode               : %d (%s)\n", best_overall.puct.book_mode, bm_str);
            printf("  - book_temperature        : %.4f\n", best_overall.puct.book_temperature);
        }
        printf("  - use_db                  : %s\n", best_overall.use_db ? "true" : "false");
        printf("  - Win Rate (Score %%)      : %.1f%% (Elo: %.0f)\n\n", best_overall.score_pct, best_overall.elo);
        
        printf("Recommended C EngineConfig preset:\n");
        printf("```c\n");
        printf("cfg->puct_c_puct = %.4ff;\n", best_overall.puct.c_puct);
        printf("cfg->puct_temperature = %.4ff;\n", best_overall.puct.puct_temperature);
        printf("cfg->puct_rollout_epsilon = %.4ff;\n", best_overall.rollout_epsilon);
        printf("cfg->puct_max_rollout_depth = %d;\n", best_overall.max_rollout_depth);
        printf("cfg->puct_use_guided_book = %s;\n", best_overall.puct.use_guided_book ? "true" : "false");
        printf("cfg->puct_lambda_book = %.4ff;\n", best_overall.puct.lambda_book);
        printf("cfg->puct_use_book = %s;\n", (!best_overall.puct.use_guided_book && best_overall.puct.book_mode != BOOK_MODE_OFF) ? "true" : "false");
        printf("cfg->book_mode = %d;\n", best_overall.puct.use_guided_book ? BOOK_MODE_OFF : best_overall.puct.book_mode);
        printf("cfg->book_temperature = %.4ff;\n", best_overall.puct.book_temperature);
        printf("cfg->puct_use_db = %s;\n", best_overall.use_db ? "true" : "false");
        printf("```\n\n");
    } else {
        const char *bm_str = (best_overall.ucb1.book_mode == BOOK_MODE_BEST) ? "BEST" :
                             (best_overall.ucb1.book_mode == BOOK_MODE_GOOD) ? "GOOD" :
                             (best_overall.ucb1.book_mode == BOOK_MODE_ALL) ? "ALL" : "OFF";

        printf("  - alpha (exploration)     : %.4f\n", best_overall.ucb1.exploration_alpha);
        printf("  - rollout_epsilon         : %.4f\n", best_overall.rollout_epsilon);
        printf("  - max_rollout_depth       : %d\n", best_overall.max_rollout_depth);
        printf("  - book_mode               : %d (%s)\n", best_overall.ucb1.book_mode, bm_str);
        printf("  - book_temperature        : %.4f\n", best_overall.ucb1.book_temperature);
        printf("  - use_db                  : %s\n", best_overall.use_db ? "true" : "false");
        printf("  - Win Rate (Score %%)      : %.1f%% (Elo: %.0f)\n\n", best_overall.score_pct, best_overall.elo);

        printf("Recommended C EngineConfig preset:\n");
        printf("```c\n");
        printf("cfg->mcts_exploration = %.4ff;\n", best_overall.ucb1.exploration_alpha);
        printf("cfg->mcts_rollout_epsilon = %.4ff;\n", best_overall.rollout_epsilon);
        printf("cfg->mcts_max_rollout_depth = %d;\n", best_overall.max_rollout_depth);
        printf("cfg->book_mode = %d;\n", best_overall.ucb1.book_mode);
        printf("cfg->mcts_use_book = %s;\n", (best_overall.ucb1.book_mode != BOOK_MODE_OFF) ? "true" : "false");
        printf("cfg->book_temperature = %.4ff;\n", best_overall.ucb1.book_temperature);
        printf("cfg->mcts_use_db = %s;\n", best_overall.use_db ? "true" : "false");
        printf("```\n\n");
    }

    if (out_result) {
        out_result->best_overall = best_overall;
        out_result->total_games_played = total_games;
        out_result->total_duration = elapsed_sec;
        out_result->final_best_score = best_overall.score_pct;
        out_result->final_best_elo = best_overall.elo;
    }

    return 0;
}
