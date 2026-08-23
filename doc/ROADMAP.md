# Damascus: Missing Features, Enhancements, and Implementation Roadmap

> **Status**: Living Specification & Implementation Roadmap  
> **Target**: Academic Project Requirements for Italian Draughts (*Dama Italiana*) Artificial Player  
> **Language**: Pure C (C11)  
> **Format**: Structured for AI Agent / Developer Execution

---

## 1. Executive Summary & Compliance Audit

Damascus is a 3D Italian Draughts game and artificial player written in pure C11 using OpenGL, GLFW, bitboards, and MCTS (Monte Carlo Tree Search).  
While the core board representation (128-bit bitboard), move generator (FID 4-tier Law of Maximum captures), WLD endgame database, and classic UCB1 engine are implemented and tested, several mandatory project requirements and architectural enhancements remain to be completed.

### Compliance Matrix

| Requirement / Component | Current Status | Action Needed |
| :--- | :--- | :--- |
| **Pure C Implementation** | ✅ Compliant (C11) | Maintain C-only standard for all new modules. |
| **Italian Draughts Rules (FID)** | ✅ Compliant | Fully validated (forced captures, quality priority). |
| **MCTS Exploration Models** | ✅ Compliant (UCB1 & PUCT) | Both UCB1 and PUCT models implemented & tested. |
| **Thinking Time Profiles** | ✅ Compliant | Presets aligned strictly to `0.2s` (Fast), `1.0s` (Medium), `3.0s` (Slow). |
| **Zobrist Hashing / Transposition** | ✅ Compliant | Implemented 64-bit Zobrist hashing & Transposition Table. |
| **Heuristic / Biased Rollouts** | ✅ Compliant | Zero-allocation $\epsilon$-greedy rollout policy with FID heuristics. |
| **Headless Tournament CLI** | ✅ Compliant | Full CLI mode for matches, round-robin tournaments, benchmarks. |
| **Principled Parameter Tuning** | ✅ Compliant (Genetic Algorithm) | Automated GA tuning framework with ODB & WLD support. |
| **Experimental Data Export** | ✅ Compliant | Automatic CSV export for matches, tournaments, and benchmarks. |

---

## 2. Ordered Implementation Roadmap

The following sections are organized in strict chronological order of execution.

```
[Phase 1: PUCT Selection Model] ✅ Completed
           ↓
[Phase 2: Zobrist Hashing & Transposition Table] ✅ Completed
           ↓
[Phase 3: Tactical / Biased Rollouts] ✅ Completed
           ↓
[Phase 4: Headless CLI & Tournament Runner] ✅ Completed
           ↓
[Phase 5: Automated Hyperparameter Tuning Engine] ✅ Completed
           ↓
[Phase 6: UI Profile & Model Selector Alignment]
           ↓
[Phase 7: Experimental Benchmarking & Report Dataset Generation]
```

---

## 3. Detailed Specifications by Phase

### Phase 1: Second Selection Policy — PUCT (Predictor Upper Confidence Tree) ✅ COMPLETED

The project specification mandates **at least two exploration/exploitation selection models**.  
The second model is **PUCT** (AlphaGo-style), incorporating prior move probabilities $P(s, a)$ computed from fast domain heuristics.

#### 1.1 Mathematical Formulation
For a node $s$ and child move $a$:

$$\text{PUCT}(s, a) = Q(s, a) + c_{\text{puct}} \cdot P(s, a) \cdot \frac{\sqrt{\sum_{b} N(s, b)}}{1 + N(s, a)}$$

Where:
- $Q(s, a) = \frac{w(s, a)}{N(s, a)}$: Empirical win rate of action $a$.
- $N(s, a)$: Visit count of child $a$.
- $c_{\text{puct}}$: Exploration constant (tunable parameter, default $\approx 1.5$).
- $P(s, a)$: Prior policy probability normalized across legal moves:
  $$P(s, a) = \frac{\exp((H(s, a) - \max_b H(s, b)) / \tau)}{\sum_{b} \exp((H(s, b) - \max_c H(s, c)) / \tau)}$$
- $H(s, a)$: Lightweight domain heuristic score for move $a$:
  - King capture: $+3.0$
  - Man capture: $+1.5$
  - Promotion move: $+2.0$
  - King move: $+0.5$
  - Advancement towards promotion rank: $+0.2 \times \Delta\text{row}$
  - Moving away from base back-rank defense: $-0.3$

#### 1.2 Architecture & Files
- Implemented in `src/mcts_puct.h`, `src/engine_mcts_puct.h`, and `src/mcts_puct.c`.
- Extended `EngineType` enum in `src/game.h` to include `ENGINE_TYPE_MCTS_PUCT` (value 4, `ENGINE_TYPE_COUNT = 5`).
- Registered PUCT engine in `src/engine.h`, `src/engine_random.c`, and `src/ui.c`.
- Added interactive PUCT configuration tab in settings UI ($c_{\text{puct}}$, $\tau$, time budget, rollout depth, WLD tablebase, console debug log).

---

### Phase 2: Zobrist Hashing & Transposition Table ✅ COMPLETED

Replace full board state comparisons with 64-bit Zobrist hashing for rapid subtree reuse, transposition detection, and repetition tracking.

#### 2.1 Hashing Specifications
- 64-bit pseudo-random keys initialized with deterministic seed (SplitMix64):
  - `g_zobrist_pieces[piece_type][sq]` (4 piece types $\times$ 32 dark squares).
  - `g_zobrist_player` (turn toggle).
- Incremental update during `game_execute_move()` via XOR operations:
  $$\text{hash}' = \text{hash} \oplus \text{zobrist\_piece}[p][\text{from}] \oplus \text{zobrist\_piece}[p'][\text{to}] \oplus \bigoplus_{c} \text{zobrist\_piece}[cap][c] \oplus \text{zobrist\_player}$$
- Fast repetition checking in `game_get_repetition_count()` via 64-bit integer comparisons on history hashes.

#### 2.2 Transposition Table in MCTS
- Dedicated high-performance Transposition Table (`src/transposition.h`, `src/transposition.c`):
  - Preallocated 524,288 entries ($\approx 8.4\text{ MB}$, zero allocations in search).
  - Stores `uint64_t key`, `uint32_t node_idx`, `uint16_t depth`, `uint16_t age`.
  - Integrated into both `MCTS UCB1` and `MCTS PUCT` selection, expansion, subtree reuse, and real-time debug HUD.

---

### Phase 3: Tactical / Biased Rollouts (Heuristic Simulation Policy) ✅ COMPLETED

Uniformly random rollouts in Draughts often generate unrealistic piece sacrifices and prolonged draw loops. Introducing a lightweight, zero-allocation $\epsilon$-greedy rollout policy dramatically increases simulation quality.

#### 3.1 Policy Specifications
- **With probability $1 - \epsilon$ (e.g., $85\%$)**: Select the move with highest fast heuristic weight $H(s, a)$.
- **With probability $\epsilon$ (e.g., $15\%$)**: Select a uniform random legal move for exploration diversity.
- Keep rollout execution allocations at **strictly zero** (in-place `GameState` mutations).

#### 3.2 Architecture & Files
- Implemented header-only zero-allocation evaluator and selector in `src/mcts_heuristic.h`.
- Integrated biased simulation loops and configurable $\epsilon_{\text{rollout}}$ into both `src/mcts_ucb1.c` and `src/mcts_puct.c`.
- Added `mcts_rollout_epsilon` and `puct_rollout_epsilon` to `EngineConfig` in `src/engine.h` and `src/engine_random.c`.
- Added interactive $\epsilon_{\text{rollout}}$ stepper controls in settings UI (`src/ui.c`).

---

### Phase 4: Headless CLI Tournament & Benchmark Runner ✅ COMPLETED

Allows executing matches and tournaments directly from the terminal without initializing GLFW/OpenGL.

#### 4.1 CLI Interface Requirements
Implemented dedicated CLI module in `src/cli.h` and `src/cli.c`, integrated into `src/main.c`:

```bash
# Run a single match between two engines (with CSV export)
./Damascus --match --white=mcts_ucb1 --black=checkerboard --time=1.0 --games=10 --csv=results/match.csv

# Run a Round-Robin tournament between all available engines
./Damascus --tournament --engines=ucb1,puct,checkerboard,kingsrow,random --time=0.2 --games-per-pair=50 --csv=results/tourney.csv

# Run speed and throughput benchmark
./Damascus --bench --budget=0.2,1.0,3.0 --csv=results/bench.csv
```

#### 4.2 Output Formats
- Real-time ANSI progress bar and move ticker to `stderr` (`\r` dynamic updates with piece counts and active turn).
- Match summary results, Cross-Table matrix, Leaderboard, plies, and timing to `stdout`.
- Detailed game-by-game CSV logging for empirical evaluation.

---

### Phase 5: Principled Hyperparameter Tuning (Genetic Algorithm Engine) ✅ COMPLETED

Automated parameter tuning framework to systematically optimize engine hyperparameters across classical MCTS UCB1, MCTS PUCT, Opening Book sampling, and WLD tablebases using fast time budgets ($0.2\text{s}$).

#### 5.1 Tunable Chromosome Parameters
| Parameter | Range | Target Model | Description |
| :--- | :--- | :--- | :--- |
| $\alpha$ (Exploration) | $[0.2, 3.0]$ | UCB1 | UCB1 Exploration-exploitation trade-off constant. |
| $c_{\text{puct}}$ | $[0.5, 3.5]$ | PUCT | Predictor confidence scaling factor. |
| $\tau$ (Temperature) | $[0.1, 2.5]$ | PUCT | Softmax prior probability distribution sharpness. |
| $\epsilon_{\text{rollout}}$ | $[0.02, 0.50]$ | Both | Exploration rate in biased heuristic simulations. |
| `max_rollout_depth` | $[20, 150]$ | Both | Simulation truncation depth limit. |
| $\tau_{\text{book}}$ (Book Temp) | $[0.1, 3.0]$ | Both | Softmax temperature for opening book move sampling. |
| `book_mode` | Categorical | Both | Selection mode (`BEST`, `GOOD`, `PUCT_GUIDED`, `ALL`, `OFF`). |
| `use_book` | Boolean | Both | Toggle opening book lookup during search. |
| `use_db` | Boolean | Both | Toggle WLD endgame tablebase solving. |

#### 5.2 Genetic Algorithm Mechanics (`src/tune_ga.h`, `src/tune_ga.c`)
1. **Population Initialization**: Size $N=16$ (configurable). Individual 0 is seeded with baseline default parameters; remaining individuals are randomly initialized within bounded parameter domains.
2. **Fitness Evaluation via Tournament**:
   - Each individual plays a round-robin tournament across pairings with alternating White/Black colors and 2-ply randomized openings for move diversity.
   - Points awarded: Win = $1.0$, Draw = $0.5$, Loss = $0.0$.
   - Fitness metric: Bayes-Elo estimation with win margin bonus and execution speed penalty.
3. **Elitism & Selection**:
   - Preserves top $E$ elite individuals (default $2$) across generations without perturbation.
   - Tournament selection ($k=2$) for parent reproduction.
4. **Crossover & Mutation**:
   - Arithmetic blend crossover (BLX-$\alpha$) for continuous parameters and discrete uniform crossover for categorical/boolean flags.
   - Gaussian/uniform perturbation with parameter clamping and mutation rate $p_m \approx 0.20$.
5. **CLI & Automation**:
   - Integrated into CLI via `--tune` / `--tune-ga` with automated CSV export and C preset generator.
   - Verified via unit test suite in `tests/test_tune_ga.c`.

---

### Phase 6: GUI Profile & Engine Configuration Alignment ✅ COMPLETED

Aligned graphical interface with responsive asynchronous engine integration and precision parameter configuration:
1. **Centralized Time Controls & Keyboard Entry**:
   - Removed redundant top-level time buttons from main menu; centralized all time controls within "Impostazioni Dettagliate Motori".
   - Preset Stepper (`-` / `+`) dynamically cycles through standard profiles:
     - `Fast`: $0.20\text{s}$
     - `Medium`: $1.00\text{s}$
     - `Slow`: $3.00\text{s}$
   - Interactive inline numeric keyboard input: clicking on time value boxes enables direct typing of custom floating-point seconds with blinking cursor and Enter/Escape support.
2. **Dropdown Engine Selectors**:
   - Replaced cycling buttons with dropdown modal dialogs featuring radio-style selectors for 1P mode (Opponent Engine) and CPU vs CPU mode (White Engine / Black Engine).
3. **Asynchronous Background Worker & Non-Blocking GUI**:
   - Engine search runs asynchronously on a dedicated worker thread (`AIWorker`), keeping GLFW/OpenGL rendering at a steady 60 FPS.
   - Moving or resizing the window no longer freezes the AI or rendering loop.
   - Instant UI responsiveness: clicking `MENU` or `MOTORI` sends immediate atomic cancellation flags (`engine_request_stop()`) to halt background computation cleanly without freezing.
4. **Live In-Game HUD**:
   - Displays real-time thinking time (`CALCOLO (1.2s)...`), node pool utilization (`POOL: 142k (14.2%)`), throughput (`950k/s`), win rate (`WIN: 58%`), and last move latency.

---

### Phase 7: Experimental Benchmarking & Academic Report Dataset

Generate empirical evidence required for the final evaluation report:
- **Experiment 1 (Time Scaling)**: Win rate of MCTS UCB1 / PUCT as a function of time budget ($0.2\text{s}$ vs $1.0\text{s}$ vs $3.0\text{s}$).
- **Experiment 2 (Policy Comparison)**: Head-to-head tournament between MCTS UCB1, MCTS PUCT, Checkerboard, and Kingsrow (at equal $0.2\text{s}$, $1.0\text{s}$, $3.0\text{s}$ limits).
- **Experiment 3 (Rollout Throughput & Ablation)**:
  - Throughput (simulations/sec) with pure random vs heuristic rollouts vs WLD tablebase cutoff.
  - Subtree reuse efficiency (hit rate and retained node count).
- Auto-export results to `results/tournament_results.csv` and summary tables.

---

## 4. Key Constraints & Non-Goals

1. **No Multithreading Required for Core Scoring**: Search algorithms should maximize single-core efficiency and bitboard throughput before adding threading.
2. **Strict Turn-Budget Compliance (No Pondering / Background Search)**: The engine must search **only** when it is its active turn, strictly stopping when the configured time budget ($0.2\text{s}, 1.0\text{s}, 3.0\text{s}$) expires.
3. **Zero Dynamic Allocation in Search Loops**: All nodes, move lists, and rollout structures must use static pre-allocated memory pools or stack memory.
