/*______________________________________________________________________________
  Damascus - WLD-Constrained Alpha-Beta Shortest-Win Mini-Solver
  Finds the optimal, shortest sequence to victory / conversion in endgame tablebase positions.
  Eliminates wandering, cyclic moves, and 1-ply lookahead limitations.
______________________________________________________________________________*/

#ifndef WLD_SOLVER_H
#define WLD_SOLVER_H

#include "game.h"
#include "wld_db.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WLD_SOLVER_DEFAULT_DEPTH 4
#define WLD_SOLVER_MAX_DEPTH     10
#define WLD_SOLVER_WIN_SCORE     100000
#define WLD_SOLVER_INFINITY      1000000

typedef struct {
    Move     best_move;
    int      score;
    int      depth_to_mate; // Plies to capture/win, or 0 for draw, or negative
    int      nodes_visited;
    WLDValue outcome;       // WLD_WIN_WHITE, WLD_WIN_BLACK, WLD_DRAW
} WLDSolverResult;

/**
 * @brief Checks whether the solver is applicable to the current game state
 * (i.e. backend is active and piece count is within tablebase limits).
 */
bool wld_solver_is_applicable(const GameState *game);

/**
 * @brief Performs an exact Alpha-Beta shortest-win search on endgame positions.
 * @param game Current game state.
 * @param max_depth Maximum search depth in plies (<= WLD_SOLVER_MAX_DEPTH, 0 for default).
 * @param debug_log If true, prints detailed search metrics and candidate evaluations.
 * @return Complete search result including best move, score, and visited node count.
 */
WLDSolverResult wld_solver_search(const GameState *game, int max_depth, bool debug_log);

/**
 * @brief Convenience helper for MCTS and CLI engines. Returns the best move directly.
 */
Move wld_solver_select_move(const GameState *game, int max_depth, bool debug_log);

#ifdef __cplusplus
}
#endif

#endif // WLD_SOLVER_H
