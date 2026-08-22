/*______________________________________________________________________________
  Damascus - WLD-Constrained Alpha-Beta Shortest-Win Mini-Solver
  Implementation of exact depth-to-mate search for endgame tablebase states.
______________________________________________________________________________*/

#include "wld_solver.h"
#include "wld_db.h"
#include "game.h"
#include "zobrist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SOLVER_TT_SIZE 16384
#define SOLVER_TT_MASK (SOLVER_TT_SIZE - 1)

#define TT_FLAG_EXACT 1
#define TT_FLAG_LOWER 2
#define TT_FLAG_UPPER 3

typedef struct {
    uint64_t hash;
    int32_t  score;
    uint8_t  depth_left;
    uint8_t  flag;
    uint8_t  wld;
    Move     best_move;
} SolverTTEntry;

static _Thread_local SolverTTEntry s_solver_tt[SOLVER_TT_SIZE];

typedef struct {
    uint64_t hash;
    uint8_t  wld;
} WLDProbeCacheEntry;

static _Thread_local WLDProbeCacheEntry s_wld_probe_cache[SOLVER_TT_SIZE];

static inline WLDValue wld_cached_probe(const GameState *game) {
    uint32_t idx = (uint32_t)(game->hash & SOLVER_TT_MASK);
    if (s_wld_probe_cache[idx].hash == game->hash && s_wld_probe_cache[idx].wld != WLD_UNKNOWN) {
        return (WLDValue)s_wld_probe_cache[idx].wld;
    }
    WLDValue val = wld_probe_state(game);
    s_wld_probe_cache[idx].hash = game->hash;
    s_wld_probe_cache[idx].wld = (uint8_t)val;
    return val;
}

static uint8_t s_sq_dist[32][32];
static bool s_dist_table_inited = false;

static void init_dist_table(void) {
    if (s_dist_table_inited) return;
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            int r1 = SQ_TO_ROW(i), c1 = SQ_TO_COL(i);
            int r2 = SQ_TO_ROW(j), c2 = SQ_TO_COL(j);
            int dr = abs(r1 - r2);
            int dc = abs(c1 - c2);
            int cheb = dr > dc ? dr : dc;
            int manh = dr + dc;
            s_sq_dist[i][j] = (uint8_t)(cheb * 10 + manh);
        }
    }
    s_dist_table_inited = true;
}

static inline int sq_chebyshev(int sq1, int sq2) {
    int r1 = SQ_TO_ROW(sq1), c1 = SQ_TO_COL(sq1);
    int r2 = SQ_TO_ROW(sq2), c2 = SQ_TO_COL(sq2);
    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);
    return dr > dc ? dr : dc;
}

static inline int sq_manhattan(int sq1, int sq2) {
    int r1 = SQ_TO_ROW(sq1), c1 = SQ_TO_COL(sq1);
    int r2 = SQ_TO_ROW(sq2), c2 = SQ_TO_COL(sq2);
    return abs(r1 - r2) + abs(c1 - c2);
}

// Calculate minimum composite distance from friendly pieces to closest enemy piece
static int calculate_pursuit_distance(const Board *board, Player attacker) {
    init_dist_table();
    uint32_t my_pieces = (attacker == PLAYER_WHITE) ?
                         BOARD_WHITE_PIECES(*board) : BOARD_BLACK_PIECES(*board);
    uint32_t opp_kings = (attacker == PLAYER_WHITE) ?
                         board->black_kings : board->white_kings;
    uint32_t opp_men   = (attacker == PLAYER_WHITE) ?
                         board->black_men : board->white_men;

    uint32_t targets = opp_kings ? opp_kings : opp_men;
    if (!targets || !my_pieces) return 0;

    int total_dist = 0;
    int max_d = 0;
    int attacker_count = 0;

    uint32_t a_mask = my_pieces;
    while (a_mask) {
        int a_sq = __builtin_ctz(a_mask);
        a_mask &= a_mask - 1;
        attacker_count++;

        int min_d = 999;
        uint32_t t_mask = targets;
        while (t_mask) {
            int t_sq = __builtin_ctz(t_mask);
            t_mask &= t_mask - 1;
            int d = s_sq_dist[a_sq][t_sq];
            if (d < min_d) min_d = d;
        }
        total_dist += min_d;
        if (min_d > max_d) max_d = min_d;
    }

    int avg_d = attacker_count > 0 ? (total_dist / attacker_count) : 0;
    return max_d * 2 + avg_d;
}

// Static endgame heuristic for depth cutoff (bounded in [-2000, +2000])
static int evaluate_static_endgame(const GameState *state, Player current_player) {
    int wm = __builtin_popcount(state->board.white_men);
    int wk = __builtin_popcount(state->board.white_kings);
    int bm = __builtin_popcount(state->board.black_men);
    int bk = __builtin_popcount(state->board.black_kings);

    int w_mat = wm * 100 + wk * 320;
    int b_mat = bm * 100 + bk * 320;

    int mat_diff = (current_player == PLAYER_WHITE) ? (w_mat - b_mat) : (b_mat - w_mat);

    // Pursuit bonus: if attacker has material advantage, reward shorter distance to opponent
    int dist_bonus = 0;
    if (mat_diff > 50) {
        int dist = calculate_pursuit_distance(&state->board, current_player);
        dist_bonus = (300 - dist); // Shorter distance gives higher score
    } else if (mat_diff < -50) {
        Player opp = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        int dist = calculate_pursuit_distance(&state->board, opp);
        dist_bonus = dist; // Defending side wants to stay far away
    }

    return mat_diff + dist_bonus;
}

typedef struct {
    Move move;
    int  sort_key; // Higher = searched earlier
} ScoredMove;

static void order_moves(const GameState *state, const MoveList *moves, Move tt_move, ScoredMove *scored) {
    init_dist_table();
    Player us = state->current_player;
    uint32_t opp_kings = (us == PLAYER_WHITE) ? state->board.black_kings : state->board.white_kings;
    uint32_t opp_men   = (us == PLAYER_WHITE) ? state->board.black_men : state->board.white_men;
    uint32_t targets   = opp_kings ? opp_kings : opp_men;

    for (int i = 0; i < moves->count; i++) {
        scored[i].move = moves->moves[i];
        int key = 0;

        if (!move_is_none(tt_move) && move_equals(moves->moves[i], tt_move)) {
            key += 100000;
        }
        if (MOVE_IS_CAP(moves->moves[i])) {
            key += 10000 + moves->moves[i].jumps * 1000;
        }
        if (MOVE_IS_PROM(moves->moves[i])) {
            key += 5000;
        }

        // Favor moving towards target pieces for attacker, staying away for defender
        if (targets != 0) {
            int to_sq = moves->moves[i].to;
            int min_d = 999;
            uint32_t t_mask = targets;
            while (t_mask) {
                int t_sq = __builtin_ctz(t_mask);
                t_mask &= t_mask - 1;
                int d = s_sq_dist[to_sq][t_sq];
                if (d < min_d) min_d = d;
            }
            int wm = __builtin_popcount(state->board.white_men) + __builtin_popcount(state->board.white_kings);
            int bm = __builtin_popcount(state->board.black_men) + __builtin_popcount(state->board.black_kings);
            bool is_attacker = (us == PLAYER_WHITE) ? (wm >= bm) : (bm >= wm);
            if (is_attacker) {
                key += (200 - min_d);
            } else {
                key += min_d;
            }
        }

        scored[i].sort_key = key;
    }

    // Simple insertion sort
    for (int i = 1; i < moves->count; i++) {
        ScoredMove temp = scored[i];
        int j = i - 1;
        while (j >= 0 && scored[j].sort_key < temp.sort_key) {
            scored[j + 1] = scored[j];
            j--;
        }
        scored[j + 1] = temp;
    }
}

// Negamax Alpha-Beta search with Transposition Table
static int wld_alpha_beta(const GameState *state, int depth, int max_depth,
                          int alpha, int beta, int *nodes_visited, Move *best_move_out) {
    (*nodes_visited)++;

    if (state->is_game_over) {
        if (state->is_draw) {
            return 0;
        }
        if (state->winner == state->current_player) {
            return WLD_SOLVER_WIN_SCORE - depth;
        } else {
            return -WLD_SOLVER_WIN_SCORE + depth;
        }
    }

    // 3-fold repetition check along the path
    if (game_get_repetition_count(state) >= 3) {
        return 0;
    }

    if (depth >= max_depth) {
        int static_eval = evaluate_static_endgame(state, state->current_player);
        return static_eval - depth;
    }

    uint64_t hash = state->hash;
    uint8_t depth_left = (uint8_t)(max_depth - depth);
    uint32_t tt_idx = (uint32_t)(hash & SOLVER_TT_MASK);
    Move tt_move = MOVE_NONE;

    if (s_solver_tt[tt_idx].hash == hash) {
        tt_move = s_solver_tt[tt_idx].best_move;
        if (s_solver_tt[tt_idx].depth_left >= depth_left) {
            int tt_score = s_solver_tt[tt_idx].score;
            if (tt_score > WLD_SOLVER_WIN_SCORE - 1000) tt_score -= depth;
            else if (tt_score < -WLD_SOLVER_WIN_SCORE + 1000) tt_score += depth;

            if (s_solver_tt[tt_idx].flag == TT_FLAG_EXACT) {
                if (best_move_out) *best_move_out = tt_move;
                return tt_score;
            } else if (s_solver_tt[tt_idx].flag == TT_FLAG_LOWER && tt_score >= beta) {
                if (best_move_out) *best_move_out = tt_move;
                return tt_score;
            } else if (s_solver_tt[tt_idx].flag == TT_FLAG_UPPER && tt_score <= alpha) {
                if (best_move_out) *best_move_out = tt_move;
                return tt_score;
            }
        }
    }

    MoveList moves = *game_get_valid_moves(state);
    if (moves.count == 0) {
        return -WLD_SOLVER_WIN_SCORE + depth;
    }

    ScoredMove scored_moves[48];
    order_moves(state, &moves, tt_move, scored_moves);

    int orig_alpha = alpha;
    int best_score = -WLD_SOLVER_INFINITY;
    Move local_best = scored_moves[0].move;

    Player us = state->current_player;
    WLDValue our_win = (us == PLAYER_WHITE) ? WLD_WIN_WHITE : WLD_WIN_BLACK;
    WLDValue our_loss = (us == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;

    for (int i = 0; i < moves.count; i++) {
        Move m = scored_moves[i].move;
        GameState child = *state;
        game_execute_move(&child, m);

        int child_score = 0;

        if (game_get_repetition_count(&child) >= 3) {
            child_score = 0;
        } else if (child.is_game_over) {
            if (child.is_draw) {
                child_score = 0;
            } else if (child.winner == us) {
                child_score = WLD_SOLVER_WIN_SCORE - (depth + 1);
            } else {
                child_score = -WLD_SOLVER_WIN_SCORE + (depth + 1);
            }
        } else {
            WLDValue wld = wld_cached_probe(&child);
            if (wld == our_win) {
                // Move maintains our winning advantage -> search recursively to find shortest conversion
                child_score = -wld_alpha_beta(&child, depth + 1, max_depth, -beta, -alpha, nodes_visited, NULL);
            } else if (wld == WLD_DRAW) {
                // Tablebase proves draw: if we were winning, this is a blunder to draw (eval = 0)
                // If we are in a draw or lost position, search recursively to maintain draw
                WLDValue state_wld = wld_cached_probe(state);
                if (state_wld == our_win) {
                    child_score = 0;
                } else {
                    child_score = -wld_alpha_beta(&child, depth + 1, max_depth, -beta, -alpha, nodes_visited, NULL);
                    if (child_score > 0) child_score = 0;
                }
            } else if (wld == our_loss) {
                // Tablebase proves opponent win: if we were winning or drawing, this is a fatal blunder
                // If we are already losing, search recursively to find maximum defensive resistance
                WLDValue state_wld = wld_cached_probe(state);
                if (state_wld == our_win || state_wld == WLD_DRAW) {
                    child_score = -WLD_SOLVER_WIN_SCORE + (depth + 1);
                } else {
                    child_score = -wld_alpha_beta(&child, depth + 1, max_depth, -beta, -alpha, nodes_visited, NULL);
                    if (child_score >= 0) child_score = -WLD_SOLVER_WIN_SCORE + (depth + 1);
                }
            } else {
                child_score = -wld_alpha_beta(&child, depth + 1, max_depth, -beta, -alpha, nodes_visited, NULL);
            }
        }

        if (child_score > best_score) {
            best_score = child_score;
            local_best = m;
        }

        if (best_score > alpha) {
            alpha = best_score;
        }

        if (alpha >= beta) {
            break;
        }
    }

    if (best_move_out) {
        *best_move_out = local_best;
    }

    // Store in TT
    if (s_solver_tt[tt_idx].hash != hash || depth_left >= s_solver_tt[tt_idx].depth_left) {
        s_solver_tt[tt_idx].hash = hash;
        s_solver_tt[tt_idx].depth_left = depth_left;
        s_solver_tt[tt_idx].best_move = local_best;

        int store_score = best_score;
        if (store_score > WLD_SOLVER_WIN_SCORE - 1000) store_score += depth;
        else if (store_score < -WLD_SOLVER_WIN_SCORE + 1000) store_score -= depth;
        s_solver_tt[tt_idx].score = store_score;

        if (best_score <= orig_alpha) {
            s_solver_tt[tt_idx].flag = TT_FLAG_UPPER;
        } else if (best_score >= beta) {
            s_solver_tt[tt_idx].flag = TT_FLAG_LOWER;
        } else {
            s_solver_tt[tt_idx].flag = TT_FLAG_EXACT;
        }
    }

    return best_score;
}

bool wld_solver_is_applicable(const GameState *game) {
    if (!game || game->is_game_over) return false;
    return wld_is_endgame_state(game);
}

static int wld_alpha_beta_root(const GameState *game, int max_depth, int *nodes_visited,
                               Move *best_move_out, bool debug_log) {
    MoveList valid_moves = *game_get_valid_moves(game);
    if (valid_moves.count == 0) return -WLD_SOLVER_WIN_SCORE;

    uint32_t tt_idx = (uint32_t)(game->hash & SOLVER_TT_MASK);
    Move tt_move = (s_solver_tt[tt_idx].hash == game->hash) ? s_solver_tt[tt_idx].best_move : MOVE_NONE;

    ScoredMove scored_moves[48];
    order_moves(game, &valid_moves, tt_move, scored_moves);

    int alpha = -WLD_SOLVER_INFINITY;
    int beta  =  WLD_SOLVER_INFINITY;

    Player us = game->current_player;
    WLDValue our_win = (us == PLAYER_WHITE) ? WLD_WIN_WHITE : WLD_WIN_BLACK;
    WLDValue our_loss = (us == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;

    Move best_move = scored_moves[0].move;
    int best_score = -WLD_SOLVER_INFINITY;

    if (debug_log) {
        int w_cnt = __builtin_popcount(game->board.white_men) + __builtin_popcount(game->board.white_kings);
        int b_cnt = __builtin_popcount(game->board.black_men) + __builtin_popcount(game->board.black_kings);
        WLDValue outcome = wld_probe_state(game);
        const char *wld_str = (outcome == WLD_WIN_WHITE) ? "VITTORIA BIANCO" :
                              (outcome == WLD_WIN_BLACK) ? "VITTORIA NERO" :
                              (outcome == WLD_DRAW) ? "PATTA TEORICA" : "SCONOSCIUTO";
        printf("\n================================================================================\n");
        printf("[WLD MINI-SOLVER - SHORTEST WIN SEARCH]\n");
        printf("Giocatore: %s | Pezzi: %d Bianco, %d Nero | Valutazione WLD: %s\n",
               (us == PLAYER_WHITE) ? "BIANCO" : "NERO", w_cnt, b_cnt, wld_str);
        printf("Profondita' massima: %d plies | Mosse candidate: %d\n", max_depth, valid_moves.count);
        printf("--------------------------------------------------------------------------------\n");
        printf("  # | Mossa            | Esito WLD  | Distanza | Ripetizioni | Score   | Plies to Win\n");
        printf("--------------------------------------------------------------------------------\n");
    }

    for (int i = 0; i < valid_moves.count; i++) {
        Move m = scored_moves[i].move;
        GameState child = *game;
        game_execute_move(&child, m);

        int child_score = 0;

        if (game_get_repetition_count(&child) >= 3) {
            child_score = 0;
        } else if (child.is_game_over) {
            if (child.is_draw) {
                child_score = 0;
            } else if (child.winner == us) {
                child_score = WLD_SOLVER_WIN_SCORE - 1;
            } else {
                child_score = -WLD_SOLVER_WIN_SCORE + 1;
            }
        } else {
            WLDValue wld = wld_cached_probe(&child);
            if (wld == our_win) {
                // Move maintains our winning advantage -> search recursively to find shortest conversion
                child_score = -wld_alpha_beta(&child, 1, max_depth, -beta, -alpha, nodes_visited, NULL);
            } else if (wld == WLD_DRAW) {
                WLDValue root_wld = wld_cached_probe(game);
                if (root_wld == our_win) {
                    child_score = 0;
                } else {
                    child_score = -wld_alpha_beta(&child, 1, max_depth, -beta, -alpha, nodes_visited, NULL);
                    if (child_score > 0) child_score = 0;
                }
            } else if (wld == our_loss) {
                WLDValue root_wld = wld_cached_probe(game);
                if (root_wld == our_win || root_wld == WLD_DRAW) {
                    child_score = -WLD_SOLVER_WIN_SCORE + 1;
                } else {
                    child_score = -wld_alpha_beta(&child, 1, max_depth, -beta, -alpha, nodes_visited, NULL);
                    if (child_score >= 0) child_score = -WLD_SOLVER_WIN_SCORE + 1;
                }
            } else {
                child_score = -wld_alpha_beta(&child, 1, max_depth, -beta, -alpha, nodes_visited, NULL);
            }
        }

        if (debug_log) {
            WLDValue child_wld = wld_cached_probe(&child);
            const char *out_str = (child_wld == our_win) ? "WIN " :
                                  (child_wld == WLD_DRAW) ? "DRAW" :
                                  (child_wld == our_loss) ? "LOSS" : "UNK ";
            int dist = calculate_pursuit_distance(&child.board, us);
            int rep = game_get_repetition_count(&child);
            int plies_to_mate = (child_score > WLD_SOLVER_WIN_SCORE - 100) ?
                                (WLD_SOLVER_WIN_SCORE - child_score) : -1;
            char mate_buf[32];
            if (plies_to_mate >= 0) {
                snprintf(mate_buf, sizeof(mate_buf), "%d plies", plies_to_mate);
            } else {
                snprintf(mate_buf, sizeof(mate_buf), "N/A");
            }

            printf(" %2d | %02d(r%d,c%d)->%02d(r%d,c%d) | %s       | dist: %3d  | rep: %d      | %7d | %s\n",
                   i + 1,
                   MOVE_FROM(m), SQ_TO_ROW(MOVE_FROM(m)), SQ_TO_COL(MOVE_FROM(m)),
                   MOVE_TO(m), SQ_TO_ROW(MOVE_TO(m)), SQ_TO_COL(MOVE_TO(m)),
                   out_str, dist, rep, child_score, mate_buf);
        }

        if (child_score > best_score) {
            best_score = child_score;
            best_move = m;
        }

        if (best_score > alpha) {
            alpha = best_score;
        }
    }

    if (best_move_out) {
        *best_move_out = best_move;
    }

    // Store root in TT
    s_solver_tt[tt_idx].hash = game->hash;
    s_solver_tt[tt_idx].depth_left = (uint8_t)max_depth;
    s_solver_tt[tt_idx].best_move = best_move;
    s_solver_tt[tt_idx].score = best_score;
    s_solver_tt[tt_idx].flag = TT_FLAG_EXACT;

    if (debug_log) {
        printf("--------------------------------------------------------------------------------\n");
        printf("Mossa scelta: %02d(r%d,c%d) -> %02d(r%d,c%d) | Score: %d | Nodi esplorati: %d\n",
               MOVE_FROM(best_move), SQ_TO_ROW(MOVE_FROM(best_move)), SQ_TO_COL(MOVE_FROM(best_move)),
               MOVE_TO(best_move), SQ_TO_ROW(MOVE_TO(best_move)), SQ_TO_COL(MOVE_TO(best_move)),
               best_score, *nodes_visited);
        printf("================================================================================\n\n");
        fflush(stdout);
    }

    return best_score;
}

WLDSolverResult wld_solver_search(const GameState *game, int max_depth, bool debug_log) {
    WLDSolverResult result;
    memset(&result, 0, sizeof(result));
    result.best_move = MOVE_NONE;
    result.score = -WLD_SOLVER_INFINITY;
    result.depth_to_mate = 0;
    result.nodes_visited = 0;
    result.outcome = WLD_UNKNOWN;

    if (!game || game->is_game_over) {
        return result;
    }

    if (max_depth <= 0) {
        max_depth = WLD_SOLVER_DEFAULT_DEPTH;
    }
    if (max_depth > WLD_SOLVER_MAX_DEPTH) {
        max_depth = WLD_SOLVER_MAX_DEPTH;
    }

    MoveList valid_moves = *game_get_valid_moves(game);
    if (valid_moves.count == 0) {
        return result;
    }

    if (valid_moves.count == 1) {
        result.best_move = valid_moves.moves[0];
        result.nodes_visited = 1;
        result.score = 0;
        result.outcome = wld_probe_state(game);
        return result;
    }

    result.outcome = wld_probe_state(game);

    // Clear Solver Transposition Table
    memset(s_solver_tt, 0, sizeof(s_solver_tt));

    // Iterative Deepening
    for (int d = 1; d <= max_depth; d++) {
        Move iter_best = MOVE_NONE;
        bool is_last_iteration = (d == max_depth);
        int iter_score = wld_alpha_beta_root(game, d, &result.nodes_visited, &iter_best, is_last_iteration ? debug_log : false);

        if (!move_is_none(iter_best)) {
            result.best_move = iter_best;
            result.score = iter_score;
        }

        // If a forced win is proven at depth d, no deeper search can find a faster win
        if (iter_score >= WLD_SOLVER_WIN_SCORE - d) {
            break;
        }
    }

    if (result.score > WLD_SOLVER_WIN_SCORE - 100) {
        result.depth_to_mate = WLD_SOLVER_WIN_SCORE - result.score;
    } else if (result.score < -WLD_SOLVER_WIN_SCORE + 100) {
        result.depth_to_mate = -(WLD_SOLVER_WIN_SCORE + result.score);
    } else {
        result.depth_to_mate = 0;
    }

    return result;
}

Move wld_solver_select_move(const GameState *game, int max_depth, bool debug_log) {
    WLDSolverResult res = wld_solver_search(game, max_depth, debug_log);
    return res.best_move;
}
