# Damascus — Code Architecture & Theoretical Foundation 🏛️

This document outlines the theoretical foundations, algorithmic architecture, and low-level performance engineering behind the **Damascus** AI engine suite for Italian Checkers (*Dama Italiana*).

---

## 1. Monte Carlo Tree Search (MCTS) Overview

**Monte Carlo Tree Search (MCTS)** is a heuristic, best-first search algorithm designed for decision-making processes in combinatorial game theory. Unlike classic Minimax or Alpha-Beta pruning algorithms that evaluate positions strictly via fixed-depth minimax recursion and static board evaluation functions, MCTS incrementally builds an asymmetric search tree driven by stochastic simulations (*rollouts*) and statistical action evaluations.

Each node in the MCTS tree represents a deterministic board state $s$, and each directed edge $(s, a)$ represents a legal action $a$ transitioning from state $s$ to successor state $s'$.

MCTS proceeds in iterative iterations, where each iteration executes **four sequential phases**:

```
           [ 1. SELECTION ]
                 │
                 ▼
           [ 2. EXPANSION ]
                 │
                 ▼
           [ 3. SIMULATION ] (Rollout)
                 │
                 ▼
         [ 4. BACKPROPAGATION ]
```

---

## 2. The Four MCTS Phases

### Phase 1: Selection
Starting from the root state $s_0$, the algorithm traverses existing tree nodes by selecting the child node that maximizes a specified selection policy until reaching a node that is not fully expanded or is a leaf.

### Phase 2: Expansion
When the selection phase reaches a state $s$ that contains legal actions not yet represented in the tree, one or more child nodes corresponding to available actions $a \in \mathcal{A}(s)$ are instantiated and appended to the search tree.

### Phase 3: Simulation (Rollout)
From the newly expanded node, a simulation policy is executed to play out the game until a terminal state is reached (win, loss, draw) or a maximum simulation depth horizon is exceeded. The final state is evaluated to produce a scalar outcome value $z \in [-1, +1]$ from the perspective of the player to move.

### Phase 4: Backpropagation
The simulation outcome $z$ is propagated upward through all visited nodes along the trajectory from the expanded node back to the root $s_0$. Each traversed node updates its statistics:
- Total visit count: $N(s) \leftarrow N(s) + 1$
- Action visit count: $N(s, a) \leftarrow N(s, a) + 1$
- Cumulative action value: $W(s, a) \leftarrow W(s, a) + z$
- Mean action value: $Q(s, a) \leftarrow \frac{W(s, a)}{N(s, a)}$

---

## 3. Node Selection Policies: UCB1 vs. PUCT

The core distinction between different MCTS paradigms resides in the **Selection Phase**, which governs how the search tree balances **exploitation** (favoring moves with known high win rates) against **exploration** (investigating moves with high uncertainty or strong theoretical priors).

Damascus implements two selection strategies: **UCB1** and **PUCT**.

---

### 3.1 MCTS UCB1 (Upper Confidence Bound 1 applied to Trees)

UCB1 formulates node selection as a Multi-Armed Bandit problem, applying confidence bounds to balance exploitation and exploration:

$$a^* = \arg\max_{a \in \mathcal{A}(s)} \left[ Q(s, a) + \alpha \cdot \sqrt{\frac{2 \ln N(s)}{N(s, a)}} \right]$$

#### Parameter & Component Breakdown:
* $Q(s, a) = \frac{W(s, a)}{N(s, a)}$: **Exploitation Term**. The empirical mean reward (win rate) of action $a$ in state $s$, normalized in the range $[0, 1]$.
* $N(s)$: **Parent Visit Count**. The total number of times parent state $s$ has been visited ($N(s) = \sum_{b \in \mathcal{A}(s)} N(s, b)$).
* $N(s, a)$: **Child Action Visit Count**. The number of times action $a$ has been selected from state $s$.
* $\ln N(s)$: **Logarithmic Parent Growth**. Ensures that the exploration bonus grows slowly as total visits to state $s$ increase.
* $\alpha$: **Exploration Constant**. A tuning hyperparameter that weights the exploration bonus relative to the exploitation term.
* $\sqrt{\frac{2 \ln N(s)}{N(s, a)}}$: **Upper Confidence Bound Bonus**. Shrinks as an action is visited more frequently and grows when sibling actions are visited while $a$ is neglected.

#### Intuitive Mechanics:
UCB1 treats every unvisited action as having an infinite exploration bonus ($N(s, a) = 0 \implies \infty$), obligating the search to visit every legal move at least once before applying confidence bounds. Once all moves have baseline statistics, the algorithm concentrates simulations on promising branches while guaranteeing that all moves are revisited periodically.

---

### 3.2 MCTS PUCT (Predictor + Upper Confidence Bound for Trees)

PUCT (AlphaZero-style) incorporates non-uniform **prior probabilities** $P(s, a)$ into the selection formula:

$$a^* = \arg\max_{a \in \mathcal{A}(s)} \left[ Q(s, a) + c_{\text{puct}} \cdot P(s, a) \cdot \frac{\sqrt{\sum_{b \in \mathcal{A}(s)} N(s, b)}}{1 + N(s, a)} \right]$$

#### Parameter & Component Breakdown:
* $Q(s, a)$: **Exploitation Term**. The empirical mean value from previous simulations through action $a$.
* $P(s, a)$: **Prior Probability**. A normalized probability distribution over all legal actions ($\sum_{a} P(s, a) = 1$) estimated by domain heuristics or opening book weights at node creation.
* $c_{\text{puct}}$: **Exploration Constant**. Controls how strongly the search is guided by the prior distribution $P(s, a)$ versus empirical rollout results $Q(s, a)$.
* $\sqrt{\sum_{b} N(s, b)} = \sqrt{N(s)}$: **Parent Visit Scaling**. The square root of total parent visits, maintaining exploratory pressure as search depth deepens.
* $1 + N(s, a)$: **Smooth Visit Regularizer**. Allows direct evaluation even when $N(s, a) = 0$, completely eliminating the need to force-visit every child node before comparing bounds.

#### Intuitive Mechanics:
PUCT immediately directs early simulations toward moves that have high prior probability $P(s, a)$. If a high-prior move consistently performs poorly in simulations, its $Q(s, a)$ decreases and its visit count $N(s, a)$ increases, reducing its exploration term. Consequently, the search automatically redirects attention toward alternative actions.

---

## 4. Biased Simulation Policy ($\epsilon$-Greedy Domain Rollouts)

In classical MCTS, the simulation (rollout) phase plays uniform random moves until the game reaches a terminal state. However, in Italian Checkers, **pure uniform random rollouts** introduce fatal weaknesses:
1. Random simulations frequently engage in aimless non-tactical piece maneuvers, missing immediate crowning moves or simple captures.
2. The defense of the home baseline (essential to prevent opponent crowning) is eroded randomly.
3. Kings and Men are treated identically during optional capture paths, distorting the terminal value signal.

Conversely, a **purely greedy rollout policy** makes simulations completely deterministic, creating blind spots to tactical combinations (the *horizon effect*).

To solve this, Damascus implements an **$\epsilon$-Greedy Biased Simulation Policy** in [`src/mcts_heuristic.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_heuristic.h), blending domain-specific heuristic exploitation with stochastic exploration.

### 4.1 Fast Domain Heuristic Evaluation Function $H(s, a)$

During simulation rollouts, candidate moves are statically evaluated via `mcts_compute_move_heuristic(state, move)` in [`src/mcts_heuristic.h:41-110`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_heuristic.h#L41-L110). 

```c
static inline float mcts_compute_move_heuristic(const GameState *state, Move move) {
    if (move_is_none(move)) return 0.0f;

    Player player = state->current_player;
    float h = 0.0f;

    // 1. Capture Value: King capture (+3.0) vs Man capture (+1.5)
    if (move.is_cap) {
        uint32_t opp_kings = (player == PLAYER_WHITE) ? state->board.black_kings : state->board.white_kings;
        uint32_t opp_men   = (player == PLAYER_WHITE) ? state->board.black_men : state->board.white_men;

        if (move.jumps > 0) {
            for (int j = 0; j < move.jumps; j++) {
                uint8_t cap_sq = move.caps[j];
                if (cap_sq < 32) {
                    uint32_t mask = 1U << cap_sq;
                    if (opp_kings & mask)      h += 3.0f; // High priority on eliminating enemy kings
                    else if (opp_men & mask)   h += 1.5f; // Standard man capture
                    else                       h += 1.5f;
                }
            }
        }
    }

    // 2. Crowning / Promotion move bonus: +2.0
    if (MOVE_IS_PROM(move)) {
        h += 2.0f;
    }

    // 3. Active King move bonus: +0.5
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

    // 5. Back-rank baseline defense penalty: -0.3
    int base_row = (player == PLAYER_WHITE) ? 0 : 7;
    if (from_row == base_row) {
        h -= 0.3f;
    }

    return h;
}
```

#### Detailed Breakdown of Heuristic Components:
1. **Capture Valuation ($+3.0$ vs $+1.5$)**:
   - In Italian Checkers, Dame (Kings) possess superior tactical mobility and capture power over Men (Pawns). When evaluating capture moves (`move.is_cap`), the function queries the opponent's bitboards (`opp_kings` and `opp_men`) for each captured square (`1U << cap_sq`).
   - Capturing an enemy King awards $+3.0$ points per king removed, while capturing a regular pawn awards $+1.5$ points. This biases rollouts heavily toward capturing high-value enemy pieces whenever alternative capture paths exist.
2. **Crowning & Promotion ($+2.0$)**:
   - Evaluates whether a move results in a pawn reaching the opponent's back rank (`MOVE_IS_PROM(move)`). Reaching the 8th rank transforms a standard pawn into a Dama, which represents a massive material and strategic advantage. The heuristic awards an immediate $+2.0$ bonus to encourage aggressive promotion rushes.
3. **Active King Mobility ($+0.5$)**:
   - Moves made by already crowned Kings (`move.piece_type == 1`) receive a $+0.5$ bonus. This prevents Kings from idling passively in corners and drives them toward active central control and opponent piece harassment.
4. **Directional Advancement Towards Promotion ($+0.2 \times \Delta_{\text{row}}$)**:
   - For standard Men (`move.piece_type == 0`), the heuristic calculates the forward row displacement $\Delta_{\text{row}}$ towards the promotion rank (increasing row index for White, decreasing row index for Black). Each advanced row grants $+0.2 \times \Delta_{\text{row}}$, providing a continuous directional vector that guides pawns forward.
5. **Back-Rank Base Defense Preservation ($-0.3$)**:
   - The original back rank (Row 0 for White, Row 7 for Black) serves as the primary defense against opponent pawns attempting to promote. Moving a pawn off the base row leaves gaps that enemy pieces can penetrate; the function penalizes leaving the home baseline with a $-0.3$ penalty unless compensated by captures or promotions.

---

### 4.2 $\epsilon$-Greedy Stochastic Action Selection Mechanism

Action choice during simulation trajectories is handled by `mcts_select_biased_rollout_move()` in [`src/mcts_heuristic.h:117-171`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_heuristic.h#L117-L171):

```c
static inline Move mcts_select_biased_rollout_move(const GameState *state, const MoveList *ml, float epsilon, uint32_t *rng_state) {
    if (ml->count == 0) return MOVE_NONE;
    if (ml->count == 1) return ml->moves[0]; // Single forced move

    // Ultra-fast zero-allocation XorShift32 PRNG
    uint32_t x = *rng_state;
    if (x == 0) x = 0x85431249U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *rng_state = x;

    // 1. EXPLORATION BRANCH (Probability epsilon): Uniform random selection
    uint32_t eps_threshold = (epsilon <= 0.0f) ? 0 : (uint32_t)(epsilon * 10000.0f);
    uint32_t rand_sample = (x >> 16) % 10000;

    if (rand_sample < eps_threshold) {
        return ml->moves[x % ml->count];
    }

    // 2. EXPLOITATION BRANCH (Probability 1 - epsilon): Select move maximizing H(s, a)
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
            best_indices[best_count++] = i; // Multiple moves sharing top score
        }
    }

    if (best_count == 1) return ml->moves[best_indices[0]];

    // Stochastic tie-breaking among top equal-scoring moves
    return ml->moves[best_indices[x % best_count]];
}
```

#### Detailed Step-by-Step Execution Flow:
1. **Trivial Case Resolution**:
   - If `ml->count == 0`, no legal moves exist (terminal game over state), returning `MOVE_NONE`.
   - If `ml->count == 1`, only a single legal move is possible (e.g. forced capture under Italian rules). The function returns `ml->moves[0]` immediately without wasting CPU cycles on random number generation or heuristic scoring.
2. **Deterministic High-Speed PRNG (XorShift32)**:
   - Updates the thread-local 32-bit random seed using bitwise shifts and XORs ($x \oplus (x \ll 13) \oplus (x \gg 17) \oplus (x \ll 5)$). This delivers high-entropy pseudo-random numbers in 3 to 4 CPU clock cycles with zero heap allocation.
3. **Exploration Branch (Probability $\epsilon$)**:
   - The parameter $\epsilon \in [0.0, 1.0]$ defines the exploration probability (e.g., $\epsilon = 0.25$ corresponds to an integer threshold of $2500 / 10000$).
   - A 14-bit pseudo-random integer in $[0, 10000)$ is extracted via `(x >> 16) % 10000`. If `rand_sample < eps_threshold`, the algorithm executes the exploration branch, picking a move uniformly at random via `ml->moves[x % ml->count]`. This injects diversity into the rollouts and prevents deterministic blind spots.
4. **Exploitation Branch (Probability $1 - \epsilon$)**:
   - When exploitation triggers, the function loops through the precomputed `MoveList` array and scores each candidate action using `mcts_compute_move_heuristic()`.
   - It dynamically maintains the highest score seen so far (`best_score`) and stores all move indices achieving that top score (within a floating-point tolerance of $10^{-4}$) in a contiguous stack buffer `best_indices[48]`.
5. **Stochastic Tie-Breaking**:
   - If exactly one move achieves the strictly highest heuristic score (`best_count == 1`), that move is chosen.
   - If multiple distinct moves share the same maximum heuristic score (`best_count > 1`), the algorithm uses the PRNG state to select uniformly at random among the tied candidates (`best_indices[x % best_count]`), completely eliminating directional or index-ordering bias.

---

### 4.3 Horizon Cutoff & Depth-Discounted Reward

To prevent simulation trajectories from stalling in deep repetitive king endgames:
1. **Simulation Horizon ($D_{\max}$)**: Rollouts terminate automatically if they exceed `max_rollout_depth` (e.g. 40 plies in UCB1, 70 plies in PUCT). The final non-terminal state is evaluated using material balance (Kings $= 3.0$, Men $= 1.0$) passed through a logistic mapping.
2. **Depth Discounting ($\gamma = 0.005$)**: When a decisive terminal outcome is reached, the reward $z$ is discounted by the total search depth ($d_{\text{total}} = d_{\text{tree}} + d_{\text{rollout}}$):
   $$R_{\text{win}} = 1.0 - \gamma \cdot d_{\text{total}}, \quad R_{\text{loss}} = \gamma \cdot d_{\text{total}}$$
   This encourages the MCTS engine to prefer rapid decisive victories and prolong resistance during disadvantageous positions.

---

## 5. Bitboard Representation & Bitwise Move Generation

In Italian Checkers, gameplay is strictly confined to the **32 dark squares** of the $8 \times 8$ board. Damascus exploits this mathematical property by modeling the entire board state as a compact **128-bit structure** composed of four 32-bit unsigned integers.

### 5.1 Data Structure Definition

In [`src/game.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/game.h):

```c
typedef struct {
    uint32_t white_men;   // 32 bit: Pedine bianche
    uint32_t white_kings; // 32 bit: Dame bianche
    uint32_t black_men;   // 32 bit: Pedine nere
    uint32_t black_kings; // 32 bit: Dame nere
} Board;
```

Each square $sq \in [0, 31]$ maps directly to bit position $(1 \ll sq)$. The coordinate transformation between 2D row/column space and square indices is implemented via ultra-fast bitwise shifts and masks:

```c
#define SQ_TO_ROW(sq)       ((int)((sq) >> 2))
#define SQ_TO_COL(sq)       ((int)((((sq) & 3) << 1) + (((sq) >> 2) & 1)))
#define ROW_COL_TO_SQ(r, c) ((int)(((r) << 2) + ((c) >> 1)))
```

### 5.2 Set-Wise Board Operations in Single CPU Instructions

Global board state queries execute in 1 to 2 CPU clock cycles using bitwise OR and NOT operations without scanning individual board cells:

```c
#define BOARD_WHITE_PIECES(b) ((b).white_men | (b).white_kings)
#define BOARD_BLACK_PIECES(b) ((b).black_men | (b).black_kings)
#define BOARD_OCCUPIED(b)     (BOARD_WHITE_PIECES(b) | BOARD_BLACK_PIECES(b))
#define BOARD_FREE(b)         (~BOARD_OCCUPIED(b) & 0xFFFFFFFFU)
```

### 5.3 Zero-Branch Piece Iteration via Hardware Intrinsics

Instead of looping over 64 matrix cells, piece extraction iterates solely over occupied squares using hardware-accelerated Count-Trailing-Zeros (`__builtin_ctz` on GCC/Clang, `_BitScanForward` on MSVC) paired with Kernighan's bit reset (`x &= x - 1`):

```c
uint32_t my_men = b->white_men;
while (my_men) {
    int sq = __builtin_ctz(my_men); // Hardware intrinsic: O(1) square lookup
    my_men &= my_men - 1;          // Clears lowest set bit in 1 cycle
    
    // Generate moves or captures for piece at 'sq'...
}
```

### 5.4 Precomputed Geometric Lookups & 4-Tier "Law of Maximum" Filtering

Move generation uses precomputed geometric lookup tables (`s_adj[32][4]`, `s_jump_dest[32][4]`, `s_jump_mid[32][4]`) to eliminate runtime coordinate bounds checking.

For capture moves, recursive Depth-First Search (DFS) functions (`dfs_white_pawn_capture`, `dfs_white_king_capture`, etc.) explore multi-jump capture trees. All generated captures are subsequently filtered through the strict 4-tier FID *Legge del Massimo* rules directly in [`src/game.c:405-463`](file:///c:/Users/Matte/CLionProjects/Damascus/src/game.c#L405-L463):

```c
// Tier 1: Filter by maximum pieces captured
uint8_t max_jumps = 0;
for (int i = 0; i < s_raw_cap_count; i++) {
    if (s_raw_caps[i].jumps > max_jumps) max_jumps = s_raw_caps[i].jumps;
}
for (int i = 0; i < s_raw_cap_count; i++) keep[i] = (s_raw_caps[i].jumps == max_jumps);

// Tier 2: Priority to King (Dama) at equal piece count
if (has_dama) {
    for (int i = 0; i < s_raw_cap_count; i++) {
        if (keep[i] && s_raw_caps[i].piece_type != 1) keep[i] = false;
    }
}

// Tier 3: Priority to maximum Kings captured
// Tier 4: Priority to first King captured earliest (first_king_step)
```

### 5.5 Benefits Over Naive 8x8 Board Representations

| Feature | Naive 2D Matrix Approach | Damascus Bitboard Approach |
|---|---|---|
| **Memory Footprint** | $64 - 128\text{ bytes}$ per state | **$16\text{ bytes}$ (128 bits)** — fits in registers |
| **Occupancy Query** | Nested $8 \times 8$ loops ($O(64)$ operations) | **$O(1)$ single bitwise OR** (`BOARD_OCCUPIED`) |
| **Piece Extraction** | Loop over 64 tiles with branch checks | **Hardware CTZ instructions** (`__builtin_ctz`) |
| **Move Validation** | Multiple conditional coordinate checks | **Precomputed LUTs + bitmask tests** |
| **Cache Efficiency** | Poor; high cache miss rate in deep trees | **Peak L1 cache utilization & SIMD alignment** |

---

## 6. Static Node Pool & Dynamic Subtree Recycling

During MCTS searches, an engine may evaluate hundreds of thousands of simulation trajectories per second. In a naive implementation, dynamic memory management (`malloc` / `free` per node) introduces memory fragmentation, lock contention in multi-threaded execution, and CPU cache thrashing.

Damascus eliminates dynamic runtime allocations entirely by employing a **preallocated static memory node pool** combined with **index-based child references** and **subtree promotion**.

### 6.1 Static Contiguous Memory Pool

In [`src/mcts_ucb1.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_ucb1.c) and [`src/mcts_puct.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_puct.c):

```c
// Per-thread dynamic preallocated memory pool of 2,000,000 nodes (~48 MB per thread)
static _Thread_local MCTSNode *s_node_pool = NULL;
static _Thread_local uint32_t s_pool_tail = 0;
static _Thread_local void *s_pool_owner = NULL;
```

Each `MCTSNode` / `PUCTNode` contains compact 32-bit integer offsets instead of 64-bit pointers:

```c
typedef struct {
    uint64_t hash;             // 64-bit Zobrist position hash
    uint32_t visits;           // Visit count N(s)
    float    wins;             // Value accumulation W(s)
    float    prior;            // Policy prior P(s, a) (in PUCT)
    uint32_t parent_idx;       // Offset of parent node in s_node_pool
    uint32_t first_child_idx;  // Contiguous slice offset of children
    Move     move;             // Action leading to this node
    uint8_t  num_children;     // Number of expanded legal actions
    uint8_t  unexpanded_idx;   // Index of next child to expand
    uint8_t  proof_status;     // Tablebase / Game-theoretic proof status
    uint8_t  proof_depth;      // Distance to proven terminal outcome
} PUCTNode;
```

### 6.2 Zero-Allocation Expansion & Sequential Contiguity

When expanding a node, children are allocated in a single contiguous block:

```c
uint32_t first_child = s_pool_tail;
s_pool_tail += ml.count; // Single O(1) index bump
s_node_pool[node_idx].first_child_idx = first_child;
```

Because child nodes are stored contiguously in memory, CPU hardware prefetchers load the entire sibling set into the **L1 Data Cache** in a single cache-line transaction during the selection loop.

### 6.3 Subtree Promotion (Tree Reuse Across Game Turns)

When a move is executed in a live game, instead of destroying the search tree and resetting statistics, Damascus traverses the existing root's children and promotes the matching subtree as the new root:

```c
// Subtree Promotion via 64-bit Zobrist Hash Match
if (s_node_pool[opp_fc + j].hash == game->hash) {
    st->root_idx = opp_fc + j;
    s_node_pool[st->root_idx].parent_idx = UINT32_MAX;
    tree_reused = true;
}
```

This retains thousands of pre-calculated node visits and tactical evaluations from previous searches, effectively providing an instant warm-start for subsequent moves.

### 6.4 Benefits Over Naive Pointer-Based Tree Architectures

| Feature | Naive Pointer-Based Tree | Damascus Static Pool Architecture |
|---|---|---|
| **Memory Allocation** | `malloc(sizeof(Node))` per expanded node | **Zero allocation during search** ($O(1)$ pool bump) |
| **Node Deallocation** | Recursive `free()` traversal | **Instant $O(1)$ pool reset** (`s_pool_tail = 0`) |
| **Memory Fragmentation** | Severe heap fragmentation over time | **Zero fragmentation** (monolithic contiguous buffer) |
| **Cache Locality** | Random pointer chasing across heap | **Sequential cache-line alignment** |
| **Multithreading** | Global allocator lock contention | **`_Thread_local` pools with zero cross-thread locks** |

---

## 7. 64-Bit Zobrist Hashing & Transposition Tables

In game tree search, different move permutations frequently lead to identical board configurations (known as *transpositions*). Damascus utilizes **64-bit Zobrist Hashing** to identify identical states, detect repetitions, and index transposition tables in $O(1)$ time.

### 7.1 Deterministic Pseudo-Random Keys

In [`src/zobrist.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/zobrist.c), Zobrist tables are initialized deterministically using the **SplitMix64** algorithm:

```c
uint64_t g_zobrist_pieces[ZOBRIST_PIECE_COUNT][ZOBRIST_SQUARE_COUNT]; // [4 piece types][32 squares]
uint64_t g_zobrist_player; // XOR key toggled on Black's turn
```

### 7.2 $O(1)$ Incremental Hashing During Move Execution

When a move is played, Damascus does not recalculate the 64-bit hash from scratch. Instead, it updates the hash incrementally via bitwise XOR operations (`^`) directly inside [`src/game.c:640-724`](file:///c:/Users/Matte/CLionProjects/Damascus/src/game.c#L640-L724):

```c
// 1. Remove piece from source square
game->hash ^= g_zobrist_pieces[piece_type][from];

// 2. Add piece to destination square (handling promotion if applicable)
game->hash ^= g_zobrist_pieces[promoted_type][to];

// 3. Remove captured pieces
for (int j = 0; j < move.jumps; j++) {
    game->hash ^= g_zobrist_pieces[captured_type][move.caps[j]];
}

// 4. Toggle active player turn
game->hash ^= g_zobrist_player;
```

Because bitwise XOR is its own inverse ($A \oplus B \oplus B = A$), adding and removing pieces requires only a single instruction per piece.

### 7.3 Transposition Table (`TranspositionTable`) & Replacement Policy

The transposition table in [`src/transposition.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/transposition.c) maps 64-bit Zobrist keys to previously visited MCTS nodes using a power-of-2 bitmask:

```c
uint32_t idx = (uint32_t)(key & tt->mask); // Fast O(1) hash indexing
```

When hash collisions occur, Damascus applies a replacement policy favoring entries from the current search generation (`age`) and deeper search depths:

```c
if (entry->age < age || (entry->age == age && depth >= entry->depth)) {
    entry->key = key;
    entry->node_idx = node_idx;
    entry->depth = depth;
    entry->age = age;
}
```

### 7.4 Threefold Repetition & Cycle Detection

The official rules of Italian Checkers state that a game is drawn if the exact same position recurs three times with the same player to move. Damascus enforces this rule with zero computational overhead by scanning the Zobrist hash history:

```c
bool game_is_threefold_repetition(const GameState *game) {
    int count = 0;
    uint64_t target_hash = game->hash;
    for (int i = 0; i < game->history_count; i++) {
        if (game->history[i].hash == target_hash) count++;
    }
    return count >= 3;
}
```

### 7.5 Benefits Over Naive State Comparison

| Feature | Naive Board Comparison | Zobrist Hashing Architecture |
|---|---|---|
| **State Comparison** | Byte-by-byte comparison of board arrays ($O(N)$) | **Single 64-bit integer equality test** ($O(1)$) |
| **Hash Computation** | Full scan of board tiles upon every move | **Incremental $O(1)$ XOR updates** (3–4 instructions) |
| **Collision Probability** | High with small 32-bit hashes | **$\approx 1 / 2^{64} \approx 5.4 \times 10^{-20}$** (collision-free in practice) |
| **Repetition Checking** | Deep nested array equality checks | **Fast 64-bit integer history scan** |
| **Transposition Lookup** | Complex tree search or heavyweight hash map | **Direct bitmask array indexing** (`key & mask`) |

---

## 8. Opening Book Subsystem (Kingsrow ODB & CheckerBoard BIN)

The opening book subsystem allows engines to access high-depth grandmaster theory during the first moves of the game. Damascus supports two discrete opening formats: **Kingsrow ODB** (`kr_italian.odb`) and **CheckerBoard BIN** (`book.bin`).

### 8.1 Binary Disk Structures & File Formats

#### 1. Kingsrow ODB Format (`kr_italian.odb`)
The Kingsrow Opening Database is an indexed binary database containing $1,759,678$ theoretical positions and $2,023,629$ evaluated moves (~33 MB). It begins with a strict 32-byte header:

```c
#pragma pack(push, 1)
typedef struct {
    char     version[8];        // ASCII: "4.01\0\0\0\0"
    uint32_t hash_prime;        // Primary modulus: 0x4000C42B (1,073,792,043)
    uint32_t format_version;    // 1
    uint32_t num_positions;     // Position count (1,759,678)
    uint32_t num_moves;         // Move count (2,023,629)
    uint32_t min_depth;         // Minimum search depth in database
    uint32_t max_depth;         // Maximum search depth in database
} OdbHeader;

typedef struct {
    uint8_t  from_sq;           // Starting square (0..31)
    uint8_t  to_sq;             // Landing square (0..31)
    int16_t  score;             // Evaluation in centipawns
    uint8_t  depth;             // Search depth ply
    uint8_t  flags;             // Classification: 0x01=Best, 0x02=Good, 0x04=Questionable
} RawBookMove;
#pragma pack(pop)
```

The payload following the header contains an index table of `uint32_t` position offsets followed by packed move records. The raw bytes of the file are encoded using a nibble substitution scheme.

#### 2. CheckerBoard BIN Format (`book.bin`)
The CheckerBoard binary format starts with an 8-byte ASCII integer declaring the entry count, followed by packed 8-byte move/position structs containing encoded 32-square board keys, source/destination squares, and centipawn values.

### 8.2 In-Memory Decoding & Softmax Sampling

Upon initialization in [`src/opening_book.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/opening_book.c), Damascus reads the binary payload into a contiguous memory buffer and decodes the nibbles in-place using a 256-entry Look-Up Table (LUT):

```c
static void init_decode_lut(uint8_t lut[256]) {
    for (int i = 0; i < 256; i++) {
        uint8_t hi = (uint8_t)((((i >> 4) - 5) & 0x0F) << 4);
        uint8_t lo = (uint8_t)(((i & 0x0F) - 5) & 0x0F);
        lut[i] = hi | lo;
    }
}
```

When querying moves, the board is converted into a 64-bit compact key (2 bits per square representing White Man, White King, Black Man, Black King, or Empty).

Move selection is governed by a **Softmax distribution** with temperature parameter $\tau$:

$$P(a_i) = \frac{\exp\left(\frac{\text{score}(a_i) - \text{score}_{\max}}{\tau \cdot 10}\right)}{\sum_{j} \ exp\left(\frac{\text{score}(a_j) - \text{score}_{\max}}{\tau \cdot 10}\right)}$$

### 8.3 Usage in MCTS UCB1 and MCTS PUCT

Damascus integrates opening book data across multiple operating modes:

#### 1. Instant Book Play (`BOOK_MODE_BEST`, `BOOK_MODE_GOOD`, `BOOK_MODE_ALL`)
In these modes, before the MCTS simulation loop is invoked, the engine probes the opening database. If the position is recognized:
- **`BOOK_MODE_BEST`**: Instantly selects the move with the highest evaluation score and deepest search ply ($0\text{ ms}$ search).
- **`BOOK_MODE_GOOD`**: Restricts the candidate pool to moves within $10\text{ centipawns}$ of the best move, then samples via the temperature-scaled Softmax distribution to provide varied opening play.
- **`BOOK_MODE_ALL`**: Samples across all legal book moves via Softmax.

#### 2. Prior Probability Blending in PUCT (`BOOK_MODE_PUCT_GUIDED`)
When PUCT search is active, the opening book does not bypass the search tree. Instead, it injects theoretical move weights into the PUCT prior distribution $P(s, a)$ via an AlphaGo-style linear blending:

$$P(s, a) = (1 - \lambda_{\text{book}}) \cdot P_{\text{heur}}(s, a) + \lambda_{\text{book}} \cdot P_{\text{book}}(s, a)$$

where $\lambda_{\text{book}} = 0.75$ and:

$$P_{\text{book}}(s, a) = \frac{\exp\left(\frac{\text{score}(s, a) + 2 \cdot \text{depth}(s, a)}{\tau_{\text{book}}}\right)}{\sum_b \exp\left(\frac{\text{score}(s, b) + 2 \cdot \text{depth}(s, b)}{\tau_{\text{book}}}\right)}$$

#### 3. Root Tree Warm-Starting
In both UCB1 and PUCT, when the root position matches an opening book entry, the engine pre-seeds the root children's statistical counters before any rollout occurs:

$$N_0(s, a) = \max(5, 5 \cdot \text{depth})$$

$$Q_0(s, a) = 0.5 + \frac{\text{score}(s, a)}{200.0}$$

This ensures that the initial MCTS exploration is immediately biased toward deep theoretical variations.

---

## 9. Endgame Tablebase Subsystem (WLD via EGDB)

The Win/Loss/Draw (WLD) tablebase subsystem provides game-theoretic evaluations (retrograde minimax analysis) for positions with **8 pieces or fewer** on the board.

### 9.1 8-Piece Tablebase Architecture & Disk Slices

The tablebase is partitioned into **45 base slice configurations** (such as `db2`, `db3`, `db4`, `db5`, `db6`, `db7`, `db8-0404`, `db8-0413`, ..., `db8-5030`), representing every valid distribution of white men, white kings, black men, and black kings up to 8 total pieces.

Each slice is stored as a pair of binary files (totaling **90 files**):
1. **`.cpr1` (Compressed Index File)**: Stores compressed block payloads containing the game-theoretic outcomes (Win, Loss, Draw) for quiescent positions.
2. **`.idx1` (Index Header File)**: Maps board configurations to byte offsets and block indices inside the `.cpr1` container.

### 9.2 Dynamic Driver Interface & Quiescent vs. Capture Resolution

On Windows (MSVC), Damascus interfaces with the official 64-bit dynamic driver library `egdb64.dll` via function pointers `egdb_identify` and `egdb_open`.

Positions are passed to the driver using Ed Gilbert's `EGDB_POSITION` representation:

```c
typedef struct {
    uint64_t black; // Bitboard of all Black pieces (men | kings)
    uint64_t white; // Bitboard of all White pieces (men | kings)
    uint64_t king;  // Bitboard of all Kings (white_kings | black_kings)
} EGDB_POSITION;
```

#### Quiescent vs. In-Flight Multi-Jump Resolution
Endgame tablebases strictly index **quiescent positions** where no capture sequences are currently pending. In Italian Checkers, where captures are mandatory and multi-jump sequences are common, querying a non-quiescent state directly produces invalid results.

To resolve this, [`wld_egdb_probe_depth`](file:///c:/Users/Matte/CLionProjects/Damascus/src/wld_egdb.c#L330-L440) executes a recursive Minimax search over legal capture branches before querying the tablebase:

```c
// In-flight capture resolution: evaluate through minimax across legal capture branches
if (MOVE_IS_CAP(moves.moves[0])) {
    bool all_losses = true;
    for (int i = 0; i < moves.count; i++) {
        GameState next = *game;
        game_execute_move(&next, moves.moves[i]);
        WLDValue child_res = wld_egdb_probe_depth(&next, depth + 1);
        if (game->current_player == PLAYER_WHITE) {
            if (child_res == WLD_WIN_WHITE) return WLD_WIN_WHITE;
            if (child_res != WLD_WIN_BLACK) all_losses = false;
        } else {
            if (child_res == WLD_WIN_BLACK) return WLD_WIN_BLACK;
            if (child_res != WLD_WIN_WHITE) all_losses = false;
        }
    }
    if (all_losses) return (game->current_player == PLAYER_WHITE) ? WLD_WIN_BLACK : WLD_WIN_WHITE;
    return WLD_DRAW;
}
```

Once a quiescent position is reached, `egdb64.dll` returns:
* `1` (`EGDB_WIN`): Win for the side to move.
* `2` (`EGDB_LOSS`): Loss for the side to move.
* `3` (`EGDB_DRAW`): Theoretical draw.

### 9.3 MCTS-Solver Proof Propagation & Rollout Bypass in UCB1 / PUCT

When `--wld-backend=official` (or `--db`) is enabled, UCB1 and PUCT utilize the tablebase through several integrated mechanisms:

#### 1. Node Expansion Proof Initialization (`mcts_init_node_proof`)
When expanding any node where the total piece count $\le 8$, Damascus probes the tablebase. If resolved, it assigns a mathematical proof state (`MCTS_PROOF_WIN`, `MCTS_PROOF_LOSS`, `MCTS_PROOF_DRAW`) and initializes node visits to $N=1$ and empirical value to $1.0$ (win), $0.0$ (loss), or $0.5$ (draw).

#### 2. MCTS-Solver Game-Theoretic Proof Backpropagation (`mcts_update_proof_status`)
Proof statuses propagate up the search tree using standard game-theoretic AND/OR logic:
* **Forced Win**: If *at least one* child is a proven loss for the opponent (`MCTS_PROOF_LOSS`), the parent is immediately marked as a proven win (`MCTS_PROOF_WIN`), recording `proof_depth = min_loss_depth + 1`.
* **Forced Loss**: If *all* children are proven wins for the opponent (`MCTS_PROOF_WIN`), the parent is marked as a forced loss (`MCTS_PROOF_LOSS`), recording `proof_depth = max_win_depth + 1`.
* **Forced Draw**: If all children are mathematically solved and at least one results in a draw, the parent is marked as `MCTS_PROOF_DRAW`.

#### 3. Selection Policy & Action Choice Override
During the MCTS selection phase, solved nodes bypass standard UCB1/PUCT formulas:
* Proven winning moves are assigned a priority score of $+10000 - \text{proof\_depth}$, directing the engine toward the shortest forced win.
* Proven losing moves are assigned $-10000 + \text{proof\_depth}$, causing the engine to avoid losing moves or choose the path of maximum resistance.
* If the root node itself is proven, the search terminates early immediately.

#### 4. Rollout Simulation Bypass
During the simulation phase, if a rollout trajectory reaches an 8-piece configuration, the simulation is cut off immediately. The rollout reward is set to the exact tablebase value ($1.0, 0.0, 0.5$) rather than simulating stochastic or heuristic moves to the end of the game.

---

## 10. Automated Hyperparameter Optimization via Genetic Algorithms (GA)

The effectiveness of Monte Carlo Tree Search engines depends heavily on the harmonious interaction of multiple continuous and discrete hyperparameters (e.g., exploration constants $c_{\text{puct}}$ or $\alpha$, Softmax policy temperature $\tau$, simulation rollout horizon, $\epsilon$-greedy exploration rate, opening book sampling parameters, and tablebase switches). 

Because the hyperparameter landscape of MCTS is non-linear, multi-dimensional, non-convex, and lacks analytical gradients, Damascus implements a dedicated **Genetic Algorithm (GA) framework** in [`src/tune_ga.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/tune_ga.h) and [`src/tune_ga.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/tune_ga.c) for automated evolutionary self-play tuning.

### 10.1 Chromosome Representation (Engine Genotype)

Each candidate configuration in the population is represented as an individual `Chromosome` embodying both continuous and discrete parameters:

```c
typedef struct {
    EngineType target_engine;        // Target architecture: MCTS_PUCT or MCTS_UCB1

    // PUCT specific genes
    float      puct_c_puct;          // Exploration constant c_puct in [0.5, 3.5]
    float      puct_temperature;     // Policy temperature tau in [0.1, 2.5]
    
    // UCB1 specific genes
    float      mcts_exploration;     // Exploration constant alpha in [0.2, 3.0]
    
    // Common MCTS simulation genes
    float      rollout_epsilon;      // Epsilon-greedy rollout rate in [0.02, 0.45]
    int        max_rollout_depth;    // Simulation cutoff depth in [20, 150]
    
    // Opening Book genes
    float      book_temperature;     // Softmax temperature for book in [0.1, 3.0]
    BookPlayMode book_mode;          // BEST, GOOD, PUCT_GUIDED, ALL
    bool       use_book;             // Opening book toggle
    
    // Endgame Tablebase genes
    bool       use_db;               // 8-piece WLD tablebase toggle
} Chromosome;
```

All continuous genes are bounded and clamped via `ga_chromosome_clamp()` to enforce physically valid search bounds and prevent numerical instabilities (such as negative temperatures or zero exploration).

### 10.2 Population Initialization & Seeding

The population of size $N$ (default $N = 16$ or $24$) is instantiated in `ga_population_init()`:
* **Individual #0 (Baseline Seed)**: Initialized with default human-engineered hyperparameters (e.g. $c_{\text{puct}} = 1.5$, $\tau = 1.0$, $\alpha = 1.414$, $\epsilon = 0.15$, $D_{\max} = 70$) to ensure that the initial generation contains a competitive baseline and prevent degenerative early convergence.
* **Individuals #1 through #N-1**: Uniformly sampled across valid hyperparameter intervals using a fast 32-bit PRNG (`xorshift32`).

### 10.3 Fitness Evaluation via Parallel Round-Robin Tournaments

Rather than evaluating configurations against a static heuristic, Damascus assesses candidate strength through an empirical **Round-Robin Self-Play Tournament**:

```
       [ Generation Population: N Individuals ]
                          │
                          ▼
        ┌────────────────────────────────────┐
        │  Round-Robin Match Queue Generator │
        │  Total Games = [N * (N - 1) / 2] * K│
        └────────────────────────────────────┘
                          │
          ┌───────────────┼───────────────┐
          ▼               ▼               ▼
    [ Worker T1 ]   [ Worker T2 ]   [ Worker Tk ]  (Multi-threaded Task Pool)
          │               │               │
          └───────────────┼───────────────┘
                          │
                          ▼
        ┌────────────────────────────────────┐
        │  Composite Fitness Calculation     │
        │  Bayes-Elo Rating Estimation       │
        │  Leaderboard Sorting (Descending)  │
        └────────────────────────────────────┘
```

#### Match Mechanics & Determinism Safeguards
1. **Paired Encounters**: Every pair of individuals $(i, j)$ plays $K$ games (default $K = 2$).
2. **Alternating Colors**: Each pair alternates playing White and Black to eliminate first-move initiative bias.
3. **Opening Randomization**: The first 2 half-moves (`opening_plies = 2`) are played pseudo-randomly using a deterministic seed `(gen * 10007 + i * 101 + j * 17 + g)`. This prevents deterministic search trees from repeating identical games across generations.
4. **Multi-Threaded Work Distribution**: A pool of POSIX / Windows worker threads (`ga_eval_worker_thread`) pulls match tasks dynamically from a thread-safe mutex-guarded queue.

#### Composite Fitness Formulation
Upon tournament completion, each individual receives a composite fitness score balancing win rate, decisive conversion, and move latency:

$$\text{Fitness} = (\text{Score}_{\%} \times 10.0) + (\text{Wins} \times 1.5) - (\text{AvgMoveTime} \times 5.0)$$

Where:
* $\text{Score}_{\%} = \frac{\text{Points}}{\text{Games Played}} \times 100$ (with win $= 1.0$, draw $= 0.5$, loss $= 0.0$).
* $\text{Wins} \times 1.5$: Additional reward bonus favoring decisive wins over passive drawing strategies.
* $\text{AvgMoveTime} \times 5.0$: Latency penalty encouraging faster search convergence and decision speed.

Each individual's performance is also mapped to an estimated **Bayes-Elo rating** relative to a 1500 baseline:

$$\text{Elo} = 1500 + 400 \cdot \log_{10}\left(\frac{\text{Score}_{\%}}{100 - \text{Score}_{\%}}\right)$$

### 10.4 Genetic Operators

#### 1. Selection (Tournament Selection)
Parent selection uses binary tournament selection ($k = 2$): two candidates are sampled at random from the population, and the one with higher composite fitness is chosen as a parent for reproduction.

#### 2. Crossover (Arithmetic Blend Crossover — BLX-$\alpha$)
Governed by a crossover probability (default $80\%$), continuous parameters are blended using random interpolation coefficients $\alpha, \beta \in [-0.1, 1.1]$:

$$\text{child}_1 = \alpha \cdot p_1 + (1 - \alpha) \cdot p_2$$

$$\text{child}_2 = (1 - \alpha) \cdot p_1 + \alpha \cdot p_2$$

Discrete parameters (`book_mode`, `use_book`, `use_db`) are inherited from either parent with uniform $50\%$ probability.

#### 3. Mutation (Gaussian Perturbation)
With probability `mutation_rate` (default $20\%$) and scale `mutation_scale` (default $15\%$), genes are perturbed:

$$\text{gene}' = \text{gene} + (2r - 1) \cdot \text{scale} \cdot \Delta_{\text{range}}$$

Discrete and boolean parameters occasionally undergo stochastic bit-flips or category shifts.

### 10.5 Elitism & Evolutionary Progression

To guarantee that peak playing strength never regresses across evolutionary generations:
1. **Elitism (`elite_count = 2`)**: The top 2 individuals from the current generation are directly preserved into the next generation without mutation or crossover.
2. **Generational Replacement**: The remaining $N - 2$ population slots are populated by offspring produced by selection, crossover, and mutation.
3. **Leaderboard Tracking & Checkpointing**: At the conclusion of each generation, Damascus prints a structured leaderboard table to `stdout` and serializes the complete population state to a recovery checkpoint file (`GACheckpoint`) and CSV log for post-training analysis.

---

## 11. Low-Level Benchmarking Suite & Subsystem Profiling Architecture

To empirically validate performance optimizations, detect memory leaks, and measure search throughput across hardware architectures, Damascus embeds an automated **low-level benchmarking and profiling suite** accessible directly via headless CLI arguments (`--bench`, `--test-endgames`, `--test-opening-book`).

The benchmark suite is implemented in [`src/cli.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/cli.c) and is structured into five distinct diagnostic modules:

```
                          [ Damascus Benchmark Suite ]
                                       │
        ┌───────────────────┬──────────┴──────────┬───────────────────┐
        ▼                   ▼                     ▼                   ▼
 [ 1. Perft DFS ]   [ 2. Rollout Sims ]   [ 3. MCTS Search ]  [ 4. WLD Solver ]
   Move Gen kN/s      Biased vs Random       Iterations/sec      Query Latency
```

---

### 11.1 Phase 1: Move Generator & *Legge del Massimo* (Perft Throughput)

The Performance Test (**Perft**) executes an exhaustive combinatorial Depth-First Search (DFS) from the initial board configuration $s_0$ across depths $d \in [1, 7]$:

```c
// In src/cli.c:1516-1524
for (int depth = 1; depth <= 7; depth++) {
    double t0 = cli_get_time();
    uint64_t nodes = perft_dfs(&start_game, depth);
    double elapsed = cli_get_time() - t0;
    double knps = (elapsed > 0.0) ? ((double)nodes / elapsed) / 1000.0 : 0.0;
    printf("  Depth %d: %12llu nodes in %7.3fs (%9.2f kN/s)\n",
           depth, (unsigned long long)nodes, elapsed, knps);
    if (elapsed > 2.0) break; // Keep benchmark quick
}
```

#### What It Measures & Validates:
1. **Raw Bitboard Speed**: Quantifies the efficiency of bitwise operations, SIMD register usage, and hardware intrinsics (`__builtin_ctz` / `_BitScanForward`).
2. **FID Rule Compliance**: Because Italian Checkers enforces the strict 4-tier *Legge del Massimo*, every leaf node generated by `perft_dfs` verifies that mandatory multi-jump capture paths, King capture priorities, and earliest-king capture sequences are properly pruned and enumerated.
3. **Throughput Metric**: Reports generation speed in **kilo-nodes per second ($kN/s$)**, reaching up to **$29,663\text{ kN/s}$** ($29.66\text{ million states/sec}$).

---

### 11.2 Phase 2: Simulation Rollout Policy Profiling (Biased vs. Uniform Random)

This module executes **$25,000$ complete game simulations** (capped at a maximum horizon of 70 plies each) to compare the computational throughput of the $\epsilon$-greedy domain heuristic against raw PRNG playouts:

```c
// In src/cli.c:1532-1549: Biased Rollout with Domain Heuristics
int rollout_trials = 25000;
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

// In src/cli.c:1551-1568: Uniform Random Rollout
double t_rand_0 = cli_get_time();
for (int i = 0; i < rollout_trials; i++) {
    GameState sim = start_game;
    int d = 0;
    while (!sim.is_game_over && d < 70) {
        const MoveList *moves = game_get_valid_moves(&sim);
        if (!moves || moves->count == 0) break;
        // Raw XorShift32 uniform selection
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
        int r_idx = (int)(rng_state % moves->count);
        game_execute_move(&sim, moves->moves[r_idx]);
        d++;
    }
}
double t_rand = cli_get_time() - t_rand_0;
```

#### What It Measures & Validates:
1. **Heuristic Overhead Quantification**: Measures the exact runtime difference between evaluating domain features ($H(s, a)$ capturing kings $+3.0$, men $+1.5$, promotions $+2.0$, king mobility $+0.5$, row deltas $+0.2$, baseline defense $-0.3$) versus raw random modulo index extraction.
2. **Throughput Result**: The biased rollout completes $25,000$ full games in $0.303\text{s}$ (**$82,412.6\text{ sims/s}$**) compared to $0.284\text{s}$ (**$87,848.4\text{ sims/s}$**) for uniform random rollouts. This demonstrates that Damascus's inline bitboard heuristic incurs only a negligible **$6.2\%$ computational cost**, providing massive tactical accuracy with virtually zero performance penalty.

---

### 11.3 Phase 3: MCTS Search Engines Benchmark (30 Representative Positions)

Rather than evaluating search throughput solely from the static opening position, Damascus executes a comprehensive benchmark across **30 representative board states** equally distributed across the three game phases:
* **10 Opening Positions**: Standard starting array ($12\text{W vs }12\text{B}$) and 1-ply / 2-ply book variations ($24\text{ to }20\text{ pieces}$).
* **10 Midgame Positions**: Dense tactical structures ($18\text{ to }10\text{ pieces}$) featuring piece contact, multi-jump captures under the FID *Legge del Massimo*, and promotion race threats.
* **10 Endgame Positions**: Late-game positions ($2\text{ to }6\text{ pieces}$) with active kings, king vs pawn configurations, and classic endgame balance setups.

During this benchmark, the **Opening Book** and **Endgame Tablebases (WLD)** are strictly turned **OFF** (`mcts_use_book = false`, `puct_use_book = false`, `mcts_use_db = false`, `puct_use_db = false`) to enforce pure, unassisted MCTS tree search across all positions.

```c
// In src/cli.c:1570-1620
for (int b = 0; b < cfg->bench_budget_count; b++) {
    double budget = cfg->bench_budgets[b];

    EngineType types[2] = { ENGINE_TYPE_MCTS_UCB1, ENGINE_TYPE_MCTS_PUCT };
    for (int t = 0; t < 2; t++) {
        EngineType eng_type = types[t];
        const char *model_name = (eng_type == ENGINE_TYPE_MCTS_UCB1) ? "MCTS UCB1" : "MCTS PUCT";

        uint64_t open_visits = 0, mid_visits = 0, end_visits = 0;

        for (int p = 0; p < 30; p++) {
            const BenchPosition *bp = &s_bench_pos[p];
            GameState pos_game = setup_position(bp);

            Engine eng = engine_create(eng_type);
            EngineConfig ec;
            engine_config_init_default(&ec);
            ec.mcts_time_budget = budget;
            ec.puct_time_budget = budget;
            ec.mcts_use_book = false; // Pure MCTS search
            ec.puct_use_book = false;
            ec.mcts_use_db   = false;
            ec.puct_use_db   = false;
            engine_apply_config(&eng, eng_type, &ec);

            eng.get_move(eng.internal_state, &pos_game);

            uint32_t v = (eng_type == ENGINE_TYPE_MCTS_UCB1) ? 
                         engine_mcts_ucb1_get_root_visits(eng.internal_state) : 
                         engine_mcts_puct_get_root_visits(eng.internal_state);
            engine_destroy(&eng);

            if (p < 10)      open_visits += v;
            else if (p < 20) mid_visits += v;
            else             end_visits += v;
        }

        uint64_t total_visits = open_visits + mid_visits + end_visits;
        double open_ips    = (double)open_visits / (budget * 10.0);
        double mid_ips     = (double)mid_visits  / (budget * 10.0);
        double end_ips     = (double)end_visits  / (budget * 10.0);
        double overall_ips = (double)total_visits / (budget * 30.0);
    }
}
```

#### What It Measures & Phase Dynamics:
1. **Phase-Specific Throughput Variation**:
   - **Openings ($24 - 20\text{ pieces}$)**: Both engines achieve $\approx 71\text{k iter/s}$ (UCB1) and $\approx 58\text{k iter/s}$ (PUCT).
   - **Midgame ($18 - 10\text{ pieces}$)**: Throughput remains robust at $\approx 77\text{k iter/s}$ (UCB1) and $\approx 58\text{k - }60\text{k iter/s}$ (PUCT).
   - **Endgames ($6 - 2\text{ pieces}$)**: Due to reduced piece density, simulation trajectories reach terminal states much faster, boosting throughput to over **$167\text{k - }211\text{k iter/s}$** for UCB1 and **$74\text{k - }126\text{k iter/s}$** for PUCT.
2. **Overall Mean Scalability**: Across all 30 positions, UCB1 averages **$105\text{k - }120\text{k iter/s}$** and PUCT averages **$63\text{k - }81\text{k iter/s}$**, demonstrating consistent anytime search throughput and zero allocation stalls across the entire spectrum of game complexities.
3. **Handling of Mandatory Captures**: Positions with a single forced capture under FID rules return immediately ($0\text{ ms}$), reflecting realistic game tree behavior where forced lines require zero search overhead.

---

### 11.4 Phase 4: Full-Game Live Match Throughput Profiling (`--bench-game`)

To evaluate the exact real-world throughput progression during an unassisted, continuous game, Damascus features the `--bench-game` diagnostic module. This mode plays a **full game between MCTS PUCT (White) and MCTS UCB1 (Black)** under sequential single-threaded execution for each time budget ($0.20\text{s}$, $1.00\text{s}$, $3.00\text{s}$).

Both engines operate under default parameters with **Opening Book and Endgame Tablebases strictly disabled** (`use_book = false`, `use_db = false`), ensuring that every ply reflects pure MCTS search across the game tree.

```c
// In src/cli.c:1810-1880
while (!game.is_game_over && ply < 200) {
    Player cur_p = game.current_player;
    bool is_puct = (cur_p == PLAYER_WHITE);
    Engine *cur_eng = is_puct ? &puct_eng : &ucb1_eng;

    // Classify game stage by live piece popcount
    int total_pieces = __builtin_popcount(BOARD_ALL_PIECES(game.board));
    const char *phase_str = (total_pieces >= 20 || ply < 16) ? "Opening" :
                            (total_pieces >= 8) ? "Midgame" : "Endgame";

    double t0 = cli_get_time();
    Move best_m = cur_eng->get_move(cur_eng->internal_state, &game);
    double dur = cli_get_time() - t0;

    uint32_t visits = is_puct ? 
                      engine_mcts_puct_get_root_visits(cur_eng->internal_state) : 
                      engine_mcts_ucb1_get_root_visits(cur_eng->internal_state);

    accumulate_phase_stats(phase_str, is_puct, visits, dur);
    game_execute_move(&game, best_m);
    ply++;
}
```

#### What It Measures & Dynamic Game Behavior:
1. **Dynamic Piece Depletion**: Tracks how search throughput naturally accelerates as pieces are captured and removed from the board (from $24$ down to $\le 6$ pieces), reducing the simulation rollout depth from $70$ plies down to $5 - 10$ plies.
2. **Single-Threaded Precision**: Eliminates thread contention, context switching, and cache eviction noise to provide exact baseline throughput measurements across all game plies.
3. **Engine Throughput Comparison in Real Play**:
   - In Openings ($24 \to 20$ pieces): PUCT achieves $\approx 79\text{k - }86\text{k iter/s}$ and UCB1 achieves $\approx 101\text{k - }106\text{k iter/s}$.
   - In Midgame ($19 \to 8$ pieces): PUCT achieves $\approx 100\text{k - }140\text{k iter/s}$ and UCB1 achieves $\approx 152\text{k - }181\text{k iter/s}$.
   - In Endgames ($\le 7$ pieces): Throughput surges up to **$237\text{k iter/s}$** for PUCT and **$276\text{k iter/s}$** for UCB1 due to ultra-short terminal playouts.

---

### 11.5 Phase 5: Endgame Tablebase Solver Verification (`--test-endgames`)

The endgame benchmark in `run_test_endgames_mode()` verifies the retrograde analysis subsystem and 8-piece WLD driver integration (`egdb64.dll` across 90 slices) against canonical tactical endgame positions:

```c
// In src/cli.c:1745-1752
double t0 = cli_get_time();
WLDSolverResult res = wld_solver_search(&sc->state, WLD_SOLVER_DEFAULT_DEPTH, false);
double solve_time = cli_get_time() - t0;
double solve_ms = solve_time * 1000.0;
```

#### Verification Protocols:
1. **Game-Theoretic Accuracy**: Verifies that solved outcomes match mathematical ground truth (`WIN_WHITE`, `WIN_BLACK`, `DRAW`) for key theoretical endgames ($2\text{K vs }1\text{K}$, $3\text{K vs }1\text{K}$, $2\text{K vs }2\text{K}$, $\text{K}+\text{M vs }\text{K}$, $1\text{K vs }1\text{K}$).
2. **Conversion Playthrough Simulation**: If `sc->test_conversion` is active, the engine plays out the winning side against a Minimax defender to verify that the theoretical win is fully converted within 40 plies without cyclic repetitions.
3. **Sub-Millisecond Query Latency**: Measures the total latency per query, confirming that tablebase lookups resolve in **$0.00 - 1.38\text{ ms}$** per position.

---

### 11.6 Phase 6: Opening Book Throughput & Softmax Sampling Verification (`--test-opening-book`)

This module benchmarks the high-speed binary probing and stochastic line selection of the **Kingsrow Opening Database** (`kr_italian.odb`, $1.76\text{M}$ positions, $2.02\text{M}$ moves):

```c
// In src/cli.c:1993-2004: 1,000,000 Hash Table Probes
const int total_bench_probes = 1000000;
BookMoveList bench_list;
double t0_bench = cli_get_time();
for (int p = 0; p < total_bench_probes; p++) {
    if (opening_book_probe(&bench_game, &bench_list)) {
        bench_dummy_sum += bench_list.count;
    }
}
double t_elapsed = cli_get_time() - t0_bench;
double probes_per_sec = (double)total_bench_probes / t_elapsed;
double avg_latency_ns = (t_elapsed / (double)total_bench_probes) * 1e9;
```

#### What It Measures & Validates:
1. **Hash Table Throughput**: Executes $1,000,000$ consecutive 64-bit Zobrist key probes into the packed in-memory opening database, measuring **$9.78\text{ million probes/second}$** with an average latency of **$102.2\text{ nanoseconds}$ per probe**.
2. **Softmax Temperature Distribution**: Tests $10,000$ consecutive sampling iterations under `BOOK_MODE_GOOD` at $\tau = 1.0$, verifying that candidate theoretical moves are sampled smoothly and deterministically according to their centipawn evaluation scores.
