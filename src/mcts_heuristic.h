#ifndef MCTS_HEURISTIC_H
#define MCTS_HEURISTIC_H

#include "game.h"
#include <stdint.h>
#include <stdbool.h>

#define MCTS_DEPTH_DISCOUNT_GAMMA 0.005f

// Game-theoretic Proof Status for MCTS-Solver
typedef enum {
    MCTS_PROOF_UNKNOWN = 0,
    MCTS_PROOF_WIN     = 1, // Proven WIN for player to move at this node
    MCTS_PROOF_LOSS    = 2, // Proven LOSS for player to move at this node
    MCTS_PROOF_DRAW    = 3  // Proven DRAW for player to move at this node
} MCTSProofStatus;

static inline float mcts_compute_depth_discounted_reward(bool is_win, bool is_loss, bool is_draw, int total_depth) {
    if (is_draw) {
        return 0.5f;
    }
    if (is_win) {
        float r = 1.0f - MCTS_DEPTH_DISCOUNT_GAMMA * (float)total_depth;
        return (r < 0.51f) ? 0.51f : r;
    }
    if (is_loss) {
        float r = MCTS_DEPTH_DISCOUNT_GAMMA * (float)total_depth;
        return (r > 0.49f) ? 0.49f : r;
    }
    return 0.5f;
}

/* Fast Domain Heuristic H(s, a) for Italian Draughts (FID rules):
   - King capture: +3.0
   - Man capture: +1.5
   - Promotion move: +2.0
   - King move: +0.5
   - Advancement towards promotion rank: +0.2 * Delta_row
   - Moving away from base back-rank defense: -0.3
*/
static inline float mcts_compute_move_heuristic(const GameState *state, Move move) {
    if (move_is_none(move)) return 0.0f;

    Player player = state->current_player;
    float h = 0.0f;

    // 1. Captures Evaluation (King vs Man)
    if (move.is_cap) {
        uint32_t opp_kings = (player == PLAYER_WHITE) ? state->board.black_kings : state->board.white_kings;
        uint32_t opp_men   = (player == PLAYER_WHITE) ? state->board.black_men : state->board.white_men;

        if (move.jumps > 0) {
            for (int j = 0; j < move.jumps; j++) {
                uint8_t cap_sq = move.caps[j];
                if (cap_sq < 32) {
                    uint32_t mask = 1U << cap_sq;
                    if (opp_kings & mask) {
                        h += 3.0f;
                    } else if (opp_men & mask) {
                        h += 1.5f;
                    } else {
                        h += 1.5f; // Fallback man capture
                    }
                }
            }
        } else if (move.cap_mask != 0) {
            uint32_t mask = move.cap_mask;
            while (mask) {
                int cap_sq = __builtin_ctz(mask);
                mask &= mask - 1;
                uint32_t sq_bit = 1U << cap_sq;
                if (opp_kings & sq_bit) {
                    h += 3.0f;
                } else {
                    h += 1.5f;
                }
            }
        } else {
            h += 1.5f;
        }
    }

    // 2. Promotion move: +2.0
    if (MOVE_IS_PROM(move)) {
        h += 2.0f;
    }

    // 3. King move: +0.5
    if (move.piece_type == 1) {
        h += 0.5f;
    }

    // 4. Advancement towards promotion rank (for Men): +0.2 * Delta_row
    int from_row = SQ_TO_ROW(move.from);
    int to_row   = SQ_TO_ROW(move.to);
    if (move.piece_type == 0) {
        int delta_row = (player == PLAYER_WHITE) ? (to_row - from_row) : (from_row - to_row);
        if (delta_row > 0) {
            h += 0.2f * (float)delta_row;
        }
    }

    // 5. Moving away from base back-rank defense: -0.3
    int base_row = (player == PLAYER_WHITE) ? 0 : 7;
    if (from_row == base_row) {
        h -= 0.3f;
    }

    return h;
}

/* Biased Rollout Move Selection (epsilon-greedy policy):
   - With probability (1 - epsilon): Greedy selection based on highest H(s, a)
   - With probability epsilon: Uniform random legal move
   - Strictly zero dynamic allocations
*/
static inline Move mcts_select_biased_rollout_move(const GameState *state, const MoveList *ml, float epsilon, uint32_t *rng_state) {
    if (ml->count == 0) {
        return MOVE_NONE;
    }
    if (ml->count == 1) {
        return ml->moves[0];
    }

    // Fast xorshift random number generator
    uint32_t x = *rng_state;
    if (x == 0) x = 0x85431249U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *rng_state = x;

    // Explore with uniform random probability epsilon
    if (epsilon >= 1.0f) {
        return ml->moves[x % ml->count];
    }

    // Compare with threshold in [0, 10000)
    uint32_t eps_threshold = (epsilon <= 0.0f) ? 0 : (uint32_t)(epsilon * 10000.0f);
    uint32_t rand_sample = (x >> 16) % 10000;

    if (rand_sample < eps_threshold) {
        return ml->moves[x % ml->count];
    }

    // Exploit: find moves with highest heuristic score H(s, a)
    float best_score = -1e9f;
    uint8_t best_indices[48];
    uint8_t best_count = 0;

    for (uint8_t i = 0; i < ml->count; i++) {
        float score = mcts_compute_move_heuristic(state, ml->moves[i]);
        if (score > best_score + 1e-4f) {
            best_score = score;
            best_indices[0] = i;
            best_count = 1;
        } else if (score >= best_score - 1e-4f) {
            best_indices[best_count++] = i;
        }
    }

    if (best_count == 1) {
        return ml->moves[best_indices[0]];
    }

    // Tie-breaking among top equal-scoring moves
    return ml->moves[best_indices[x % best_count]];
}

#endif // MCTS_HEURISTIC_H
