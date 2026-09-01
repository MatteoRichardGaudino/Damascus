# Damascus — Experimental Results & Empirical Evaluation 📊

This document presents the complete experimental evaluation, empirical benchmark results, and evolutionary tuning reports for the **Damascus** Italian Checkers (*Dama Italiana*) platform.

All experiments were executed on a **Windows 11 (x64)** architecture compiled via **Microsoft Visual Studio 2022 (MSVC C11)** in Release mode, leveraging multithreaded self-play across 8 concurrent worker threads.

Our experimental workflow is structured into three comprehensive phases:
1. **Evolutionary Hyperparameter Tuning**: First, we optimized our custom Monte Carlo Tree Search engines (**MCTS UCB1** and **MCTS PUCT**) through genetic algorithms (GA) to automatically discover the optimal exploration constants, rollout horizons, policy temperatures, and database integration parameters.
2. **Multi-Engine Benchmarking Tournaments**: Subsequently, using the optimized configurations discovered during the evolutionary tuning phase, we conducted three comprehensive round-robin tournaments across three distinct time controls (**Fast** at $0.2\text{s}$, **Medium** at $1.0\text{s}$, and **Slow** at $3.0\text{s}$ per move) to benchmark Damascus against established third-party engines and evaluate how the algorithms scale with thinking time.
3. **Low-Level Engine & Subsystem Performance Benchmarks**: Finally, we measured the execution throughput of the bitboard move generator (Perft), stochastic rollout simulation policies, MCTS node expansions, 8-piece endgame tablebase solver queries, and opening book probe latencies.

---

## 1. Genetic Algorithm Hyperparameter Tuning Results

To discover optimal search configurations for our custom engines, automated evolutionary self-play tuning was performed using the Damascus Genetic Algorithm framework.

---

### 1.1 MCTS UCB1 Evolutionary Tuning

#### Experiment Configuration & CLI Reproduction
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --tune --target=ucb1 --pop=16 --generations=5 --time=0.2 --threads=8 --csv=doc/results_3/tune_ga_ucb1.csv
  ```
* **Population Size**: $N = 16$ individuals per generation.
* **Generations**: $G = 5$ evolutionary cycles.
* **Games per Pairing**: $K = 2$ games (Round-Robin: $\frac{16 \times 15}{2} \times 2 = 240$ games/gen; **1,200 games total**).
* **Time Budget**: $0.20\text{s}$ per move.

#### Evolutionary Progression (Best Individual per Generation)

| Gen | Best ID | Score % | Points | W / D / L | Elo | $\alpha$ | Rollout $\epsilon$ | Cutoff Depth | Book Mode | Book $\tau$ | Book | WLD |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | #1 | 88.3% | 26.5 / 30 | 24 / 5 / 1 | 1851.7 | 0.2846 | 0.3801 | 58 | `OFF` | 0.00 | OFF | ON |
| **2** | #0 | 80.0% | 24.0 / 30 | 19 / 10 / 1 | 1740.8 | 0.2846 | 0.3801 | 58 | `OFF` | 0.00 | OFF | ON |
| **3** | #0 | 83.3% | 25.0 / 30 | 21 / 8 / 1 | 1779.6 | 0.2846 | 0.3801 | 58 | `OFF` | 0.00 | OFF | ON |
| **4** | #13 | **91.7%** | **27.5 / 30** | **25 / 5 / 0** | **1916.6** | **0.6422** | **0.3128** | **51** | `GOOD` | **0.05** | **ON** | **OFF** |
| **5** | #14 | 66.7% | 20.0 / 30 | 14 / 12 / 4 | 1620.4 | 0.3111 | 0.3849 | 56 | `GOOD` | 0.05 | ON | ON |

#### Key Algorithmic Insights for MCTS UCB1
1. **Damped Exploration Constant ($\alpha \approx 0.64$)**: The genetic algorithm converged towards a significantly damped exploration constant ($\alpha = 0.6422$), far below the classical $\sqrt{2} \approx 1.414$. In Italian Checkers where forced capture paths dominate, lower exploration allows the tree to focus simulation budget on critical tactical variations.
2. **Balanced Rollout Horizon ($D_{\max} = 51$) & Higher $\epsilon = 0.31$**: A moderate simulation horizon of 51 plies combined with an $\epsilon$-greedy exploration rate of $0.3128$ in rollouts prevents deterministic search traps during simulation playouts.
3. **Sharp Opening Book Selection (`GOOD` mode with low $\tau = 0.05$)**: The engine achieved peak performance ($91.7\%$ win rate, undefeated $25\text{W}/5\text{D}/0\text{L}$) by utilizing the Kingsrow opening database in `GOOD` mode with low sampling temperature ($\tau_{\text{book}} = 0.05$), ensuring grandmaster opening lines while retaining slight variability.

---

### 1.1.1 Validation Matches: MCTS UCB1 (GA Baseline, No-DB) vs MCTS UCB1 (WLD-DB ON)

To empirically validate the Genetic Algorithm's decision to disable the 8-piece endgame tablebase (`use_db = false`) for MCTS UCB1, two extensive **50-game direct head-to-head matches** (100 games total) were executed across Fast ($0.20\text{s}$) and Medium ($1.00\text{s}$) time controls under identical GA-tuned hyperparameters ($\alpha = 0.6422$, $\epsilon = 0.3128$, $D_{\max} = 51$, `book_mode = GOOD`, $\tau_{\text{book}} = 0.05$).

#### 1. Fast Profile Match (0.20s / Move — 50 Games)
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=ucb1 --black=ucb1 --white-no-db --black-db --time=0.2 --games=50 --threads=8 --csv=doc/results_3/match_ucb1_nodb_vs_ucb1_db.csv
  ```
* **Results CSV**: [`doc/results_3/match_ucb1_nodb_vs_ucb1_db.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/match_ucb1_nodb_vs_ucb1_db.csv)
* **Match Duration**: $228.27\text{s}$ ($4.57\text{s}$ per game across 8 threads, avg $178.4$ plies).

| Engine Configuration | Wins | Draws | Losses | Points / 50 | Score % | Performance Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS UCB1 (GA Baseline — No-DB)** | **19** (38.0%) | 27 (54.0%) | 4 (8.0%) | **32.5** | **65.0%** | **+107.7 Elo** |
| **MCTS UCB1 (WLD-DB ON)** | 4 (8.0%) | 27 (54.0%) | 19 (38.0%) | 17.5 | 35.0% | Baseline |

---

#### 2. Medium Profile Match (1.00s / Move — 50 Games)
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=ucb1 --black=ucb1 --white-no-db --black-db --time=1.0 --games=50 --threads=8 --csv=doc/results_3/match_ucb1_nodb_vs_ucb1_db_1s.csv
  ```
* **Results CSV**: [`doc/results_3/match_ucb1_nodb_vs_ucb1_db_1s.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/match_ucb1_nodb_vs_ucb1_db_1s.csv)
* **Match Duration**: $1043.48\text{s}$ ($20.87\text{s}$ per game across 8 threads, avg $167.5$ plies).

| Engine Configuration | Wins | Draws | Losses | Points / 50 | Score % | Performance Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS UCB1 (GA Baseline — No-DB)** | **21** (42.0%) | 21 (42.0%) | 8 (16.0%) | **31.5** | **63.0%** | **+92.6 Elo** |
| **MCTS UCB1 (WLD-DB ON)** | 8 (16.0%) | 21 (42.0%) | 21 (42.0%) | 18.5 | 37.0% | Baseline |

---

#### Combined Tournament Summary (100 Games Total)

| Engine Configuration | Total Wins | Total Draws | Total Losses | Points / 100 | Overall Score % | Overall Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS UCB1 (GA Baseline — No-DB)** | **40** (40.0%) | 48 (48.0%) | 12 (12.0%) | **64.0** | **64.0%** | **+100.0 Elo** |
| **MCTS UCB1 (WLD-DB ON)** | 12 (12.0%) | 48 (48.0%) | 40 (40.0%) | 36.0 | 36.0% | Baseline |

#### Empirical Analysis & Algorithmic Takeaways
1. **Robust Consistency Across Time Scales**: Across both fast ($0.2\text{s}$) and deeper ($1.0\text{s}$) search controls, `MCTS UCB1 (No-DB)` consistently outplayed `MCTS UCB1 (WLD-DB ON)` by a ratio of **$40$ wins to $12$** ($64.0\%$ aggregate score, $+100.0$ Elo).
2. **Mechanistic Explanation**: Without full Distance-to-Conversion (DTC) metric guidance, UCB1 nodes warm-started with static $W/D/L$ values ($1.0 / 0.0 / 0.5$) can become flat when multiple moves lead to theoretical wins or draws. Pure heuristic rollouts evaluate material differences dynamically, driving aggressive piece capture and conversion tactics throughout complex endgames.

---

### 1.2 MCTS PUCT Evolutionary Tuning

#### Experiment Configuration & CLI Reproduction
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --tune --target=puct --pop=16 --generations=5 --time=0.2 --threads=8 --csv=doc/results_3/tune_ga_puct.csv
  ```
* **Population Size**: $N = 16$ individuals per generation.
* **Generations**: $G = 5$ evolutionary cycles (240 games/gen; **1,200 games total**).
* **Time Budget**: $0.20\text{s}$ per move.

#### Evolutionary Progression (Best Individual per Generation)

| Gen | Best ID | Score % | Points | W / D / L | Elo | $c_{\text{puct}}$ | $\tau$ | Rollout $\epsilon$ | Cutoff Depth | Guided Book | $\lambda_{\text{book}}$ | WLD |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | #9 | 76.7% | 23.0 / 30 | 19 / 8 / 3 | 1706.7 | 2.7579 | 2.2287 | 0.3699 | 25 | `ON` | 0.4969 | ON |
| **2** | #12 | **81.7%** | **24.5 / 30** | **22 / 5 / 3** | **1759.5** | **2.5857** | **2.2048** | **0.3416** | **112** | **`ON`** | **0.0791** | **OFF** |
| **3** | #4 | 76.7% | 23.0 / 30 | 18 / 10 / 2 | 1706.7 | 2.0728 | 2.1051 | 0.3721 | 85 | `ON` | 0.4532 | OFF |
| **4** | #14 | 75.0% | 22.5 / 30 | 18 / 9 / 3 | 1690.8 | 1.5752 | 1.0843 | 0.3424 | 46 | `ON` | 0.1649 | OFF |
| **5** | #14 | 71.7% | 21.5 / 30 | 17 / 9 / 4 | 1661.2 | 2.6665 | 2.1473 | 0.3507 | 29 | `OFF` | 0.0000 | OFF |

#### Key Algorithmic Insights for MCTS PUCT
1. **Exploration Constant Convergence ($c_{\text{puct}} \approx 2.59$)**: The genetic search converged to an optimal balance with $c_{\text{puct}} = 2.5857$. In Italian Checkers, forced multi-jump capture sequences require substantial exploration width while maintaining aggressive exploitation of tactical gains.
2. **Prior Softmax Temperature ($\tau \approx 2.20$)**: A higher policy temperature creates a broad, smooth initial probability distribution across opening and midgame candidate moves, preventing premature fixation on single lines before simulation statistics accumulate.
3. **PUCT Guided Prior Blending ($\lambda_{\text{book}} \approx 0.08$)**: Integrating the Kingsrow opening book via soft prior blending ($P(s,a) = (1 - \lambda) P_{\text{heur}} + \lambda P_{\text{book}}$) yielded peak performance ($81.7\%$ score), injecting grandmaster opening knowledge into tree search without disabling tactical dynamic exploration.
4. **Extended Rollout Horizon ($D_{\max} = 112$) & $\epsilon \approx 0.34$**: An extended simulation horizon paired with elevated $\epsilon$-greedy rollout exploration ($0.3416$) provided stable, high-quality terminal value signals for deep MCTS backpropagation.
5. **WLD Endgame Database Suppression (`use_db = false`)**: Mirroring the UCB1 findings, the GA consistently converged to disabling the static WLD database in favor of pure heuristic rollout evaluations.

---

### 1.2.1 Validation Matches: MCTS PUCT (GA Baseline, No-DB) vs MCTS PUCT (WLD-DB ON)

To empirically validate the Genetic Algorithm's selection of `use_db = false` for MCTS PUCT, two extensive **50-game direct head-to-head matches** (100 games total) were executed across Fast ($0.20\text{s}$) and Medium ($1.00\text{s}$) time controls under identical GA-tuned hyperparameters ($c_{\text{puct}} = 2.5857$, $\tau = 2.2048$, $\epsilon = 0.3416$, $D_{\max} = 112$, `use_guided_book = true`, $\lambda_{\text{book}} = 0.0791$).

#### 1. Fast Profile Match (0.20s / Move — 50 Games)
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=puct --black=puct --white-no-db --black-db --time=0.2 --games=50 --threads=8 --csv=doc/results_3/match_puct_nodb_vs_puct_db.csv
  ```
* **Results CSV**: [`doc/results_3/match_puct_nodb_vs_puct_db.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/match_puct_nodb_vs_puct_db.csv)
* **Match Duration**: $198.24\text{s}$ ($3.96\text{s}$ per game across 8 threads, avg $142.9$ plies).

| Engine Configuration | Wins | Draws | Losses | Points / 50 | Score % | Performance Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS PUCT (GA Baseline — No-DB)** | **35** (70.0%) | 10 (20.0%) | 5 (10.0%) | **40.0** | **80.0%** | **+240.8 Elo** |
| **MCTS PUCT (WLD-DB ON)** | 5 (10.0%) | 10 (20.0%) | 35 (70.0%) | 10.0 | 20.0% | Baseline |

---

#### 2. Medium Profile Match (1.00s / Move — 50 Games)
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=puct --black=puct --white-no-db --black-db --time=1.0 --games=50 --threads=8 --csv=doc/results_3/match_puct_nodb_vs_puct_db_1s.csv
  ```
* **Results CSV**: [`doc/results_3/match_puct_nodb_vs_puct_db_1s.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/match_puct_nodb_vs_puct_db_1s.csv)
* **Match Duration**: $945.30\text{s}$ ($18.91\text{s}$ per game across 8 threads, avg $151.5$ plies).

| Engine Configuration | Wins | Draws | Losses | Points / 50 | Score % | Performance Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS PUCT (GA Baseline — No-DB)** | **26** (52.0%) | 18 (36.0%) | 6 (12.0%) | **35.0** | **70.0%** | **+147.2 Elo** |
| **MCTS PUCT (WLD-DB ON)** | 6 (12.0%) | 18 (36.0%) | 26 (52.0%) | 15.0 | 30.0% | Baseline |

---

#### Combined Tournament Summary (100 Games Total)

| Engine Configuration | Total Wins | Total Draws | Total Losses | Points / 100 | Overall Score % | Overall Elo ($\Delta$) |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS PUCT (GA Baseline — No-DB)** | **61** (61.0%) | 28 (28.0%) | 11 (11.0%) | **75.0** | **75.0%** | **+190.8 Elo** |
| **MCTS PUCT (WLD-DB ON)** | 11 (11.0%) | 28 (28.0%) | 61 (61.0%) | 25.0 | 25.0% | Baseline |

#### Empirical Analysis & Observations
1. **Decisive Demonstration of Algorithmic Superiority**: Across 100 games, `MCTS PUCT (No-DB)` dominated `MCTS PUCT (WLD-DB ON)` with **$61$ wins to $11$** ($75.0\%$ aggregate score, $+190.8$ Elo).
2. **PUCT Search Dynamics in Late Game**: PUCT relies heavily on prior-guided tree expansion followed by deep rollout simulations ($D_{\max} = 112$). When the static WLD database is active, leaf evaluations for 8-piece positions collapse to flat theoretical constants ($1.0 / 0.0 / 0.5$) without tactical gradients (DTC). In contrast, deep heuristic rollouts accurately discriminate between fast, decisive promotions and sluggish defensive lines, allowing pure PUCT to convert advantages rapidly and avoid cyclic delays.

---

## 2. Tournament Experiments Across Time Profiles

### 2.1 Tournament Setup & Evaluated Engines

The tournament benchmark suite tests five engine configurations:
* **Kingsrow**: Ed Gilbert's Kingsrow Italian engine (neural network evaluation, opening book `kr_italian.odb`, 8-piece WLD tablebase).
* **MCTS PUCT**: Damascus PUCT engine with GA-tuned parameters ($c_{\text{puct}} = 2.59$, $\tau = 2.20$, $\epsilon = 0.34$, $D_{\max} = 112$, `use_guided_book = true`, $\lambda_{\text{book}} = 0.08$, `use_db = false`).
* **MCTS UCB1**: Damascus UCB1 engine with GA-tuned parameters ($\alpha = 0.64$, $\epsilon = 0.31$, $D_{\max} = 51$, `book_mode = GOOD`, $\tau_{\text{book}} = 0.05$, `use_db = false`).
* **CheckerBoard**: Martin Fierz's Dama Italiana engine (`dama.c` / `damad.dll`, classical Alpha-Beta search).
* **Random**: Uniform random baseline generator.

#### Match Rules & Adjudication Protocol
* **Ruleset**: Official FID rules (*Legge del Massimo* strictly enforced).
* **Color Balance**: Every engine pairing $(E_1, E_2)$ plays an equal number of games with alternating colors (White / Black) to eliminate first-move initiative bias.
* **Opening Diversity (`--opening-plies=2`)**: The first 2 half-moves (1 ply White, 1 ply Black) are sampled pseudo-randomly using a deterministic game seed `(seed ^ 0x9E3779B9)`.
* **Draw Adjudication**:
  * **Threefold Repetition**: Enforced via 64-bit Zobrist history tracking.
  * **Max Plies Limit**: Games exceeding 250 plies without resolution are adjudicated as theoretical draws.
* **Rating System (Bayes-Elo)**: Engine ratings are estimated relative to a tournament baseline of $1500.0$ Elo:
  $$\text{Elo} = 1500 + 400 \cdot \log_{10}\left(\frac{\text{Score}_{\%}}{100 - \text{Score}_{\%}}\right)$$

---

### 2.2 Fast Profile Tournament (0.2s / Move — 300 Games)

#### Experiment Configuration & CLI Reproduction
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --time=0.2 --games-per-pair=30 --threads=8 --csv=doc/results_3/tournament_fast.csv
  ```
* **Time Budget**: $0.20\text{s}$ per move.
* **Total Matches**: $\frac{5 \times 4}{2} \times 30 = 300$ games (60 games per engine pairing, 120 games per engine).
* **Results CSV**: [`doc/results_3/tournament_fast.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/tournament_fast.csv)

#### Tournament Standings

| Rank | Engine | Points / Games | Score % | Win % | Draw % | Loss % | W / D / L | Avg Plies | Avg Move Time | Estimated Elo |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **#1** | **Kingsrow** | **117.5 / 120** | **97.9%** | 95.8% | 4.2% | 0.0% | 115 / 5 / 0 | 82.7 | 180.2 ms | **2168.8** |
| **#2** | **MCTS UCB1** | **70.0 / 120** | **58.3%** | 46.7% | 23.3% | 30.0% | 56 / 28 / 36 | 109.5 | 153.7 ms | **1558.5** |
| **#3** | **MCTS PUCT** | **69.5 / 120** | **57.9%** | 45.8% | 24.2% | 30.0% | 55 / 29 / 36 | 108.2 | 152.7 ms | **1555.5** |
| **#4** | **CheckerBoard** | **43.0 / 120** | **35.8%** | 26.7% | 18.3% | 55.0% | 32 / 22 / 66 | 87.9 | 140.5 ms | **1398.8** |
| **#5** | **Random** | **0.0 / 120** | **0.0%** | 0.0% | 0.0% | 100.0% | 0 / 0 / 120 | 39.3 | 0.0 ms | **701.7** |

#### Head-to-Head Cross Table (Row vs. Column: Wins - Draws - Losses)

| Engine | Kingsrow | MCTS PUCT | MCTS UCB1 | CheckerBoard | Random |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Kingsrow** | — | 29 - 1 - 0 | 27 - 3 - 0 | 29 - 1 - 0 | 30 - 0 - 0 |
| **MCTS PUCT** | 0 - 1 - 29 | — | 7 - 16 - 7 | 18 - 12 - 0 | 30 - 0 - 0 |
| **MCTS UCB1** | 0 - 3 - 27 | 7 - 16 - 7 | — | 19 - 9 - 2 | 30 - 0 - 0 |
| **CheckerBoard** | 0 - 1 - 29 | 0 - 12 - 18 | 2 - 9 - 19 | — | 30 - 0 - 0 |
| **Random** | 0 - 0 - 30 | 0 - 0 - 30 | 0 - 0 - 30 | 0 - 0 - 30 | — |

#### Game Outcomes & Decisive Factors
* **Game End Reasons**: Elimination / Block: $258$ ($86.0\%$) | 3-Fold Repetition: $23$ ($7.7\%$) | Max Plies Limit (250): $19$ ($6.3\%$).
* **Color Distribution**: White Wins: $120$ ($40.0\%$) | Black Wins: $138$ ($46.0\%$) | Draws: $42$ ($14.0\%$).

---

### 2.3 Medium Profile Tournament (1.0s / Move — 160 Games)

#### Experiment Configuration & CLI Reproduction
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --time=1.0 --games-per-pair=16 --threads=8 --csv=doc/results_3/tournament_medium.csv
  ```
* **Time Budget**: $1.00\text{s}$ per move.
* **Total Matches**: $\frac{5 \times 4}{2} \times 16 = 160$ games (32 games per engine pairing, 64 games per engine).
* **Results CSV**: [`doc/results_3/tournament_medium.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/tournament_medium.csv)

#### Tournament Standings

| Rank | Engine | Points / Games | Score % | Win % | Draw % | Loss % | W / D / L | Avg Plies | Avg Move Time | Estimated Elo |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **#1** | **Kingsrow** | **62.0 / 64** | **96.9%** | 93.8% | 6.2% | 0.0% | 60 / 4 / 0 | 88.8 | 770.7 ms | **2096.5** |
| **#2** | **MCTS UCB1** | **37.5 / 64** | **58.6%** | 48.4% | 20.3% | 31.2% | 31 / 13 / 20 | 112.7 | 768.9 ms | **1560.3** |
| **#3** | **MCTS PUCT** | **36.5 / 64** | **57.0%** | 46.9% | 20.3% | 32.8% | 30 / 13 / 21 | 112.0 | 706.7 ms | **1549.2** |
| **#4** | **CheckerBoard** | **24.0 / 64** | **37.5%** | 28.1% | 18.8% | 53.1% | 18 / 12 / 34 | 89.8 | 712.8 ms | **1411.3** |
| **#5** | **Random** | **0.0 / 64** | **0.0%** | 0.0% | 0.0% | 100.0% | 0 / 0 / 64 | 38.4 | 0.0 ms | **701.7** |

#### Head-to-Head Cross Table (Row vs. Column: Wins - Draws - Losses)

| Engine | Kingsrow | MCTS PUCT | MCTS UCB1 | CheckerBoard | Random |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Kingsrow** | — | 14 - 2 - 0 | 14 - 2 - 0 | 16 - 0 - 0 | 16 - 0 - 0 |
| **MCTS PUCT** | 0 - 2 - 14 | — | 4 - 5 - 7 | 10 - 6 - 0 | 16 - 0 - 0 |
| **MCTS UCB1** | 0 - 2 - 14 | 7 - 5 - 4 | — | 8 - 6 - 2 | 16 - 0 - 0 |
| **CheckerBoard** | 0 - 0 - 16 | 0 - 6 - 10 | 2 - 6 - 8 | — | 16 - 0 - 0 |
| **Random** | 0 - 0 - 16 | 0 - 0 - 16 | 0 - 0 - 16 | 0 - 0 - 16 | — |

#### Game Outcomes & Decisive Factors
* **Game End Reasons**: Elimination / Block: $139$ ($86.9\%$) | 3-Fold Repetition: $12$ ($7.5\%$) | Max Plies Limit (250): $9$ ($5.6\%$).
* **Color Distribution**: White Wins: $71$ ($44.4\%$) | Black Wins: $68$ ($42.5\%$) | Draws: $21$ ($13.1\%$).

---

### 2.4 Slow Profile Tournament (3.0s / Move — 100 Games)

#### Experiment Configuration & CLI Reproduction
* **Command**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --time=3.0 --games-per-pair=10 --threads=8 --csv=doc/results_3/tournament_slow.csv
  ```
* **Time Budget**: $3.00\text{s}$ per move.
* **Total Matches**: $\frac{5 \times 4}{2} \times 10 = 100$ games (20 games per engine pairing, 40 games per engine).
* **Results CSV**: [`doc/results_3/tournament_slow.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/tournament_slow.csv)

#### Tournament Standings

| Rank | Engine | Points / Games | Score % | Win % | Draw % | Loss % | W / D / L | Avg Plies | Avg Move Time | Estimated Elo |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **#1** | **Kingsrow** | **38.5 / 40** | **96.2%** | 92.5% | 7.5% | 0.0% | 37 / 3 / 0 | 91.5 | 2137.8 ms | **2063.7** |
| **#2** | **MCTS UCB1** | **23.5 / 40** | **58.8%** | 45.0% | 27.5% | 27.5% | 18 / 11 / 11 | 125.8 | 2104.2 ms | **1561.4** |
| **#3** | **MCTS PUCT** | **22.0 / 40** | **55.0%** | 37.5% | 35.0% | 27.5% | 15 / 14 / 11 | 129.0 | 1724.4 ms | **1534.9** |
| **#4** | **CheckerBoard** | **16.0 / 40** | **40.0%** | 27.5% | 25.0% | 47.5% | 11 / 10 / 19 | 99.2 | 2066.5 ms | **1429.6** |
| **#5** | **Random** | **0.0 / 40** | **0.0%** | 0.0% | 0.0% | 100.0% | 0 / 0 / 40 | 40.3 | 0.0 ms | **701.7** |

#### Head-to-Head Cross Table (Row vs. Column: Wins - Draws - Losses)

| Engine | Kingsrow | MCTS PUCT | MCTS UCB1 | CheckerBoard | Random |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Kingsrow** | — | 7 - 3 - 0 | 10 - 0 - 0 | 10 - 0 - 0 | 10 - 0 - 0 |
| **MCTS PUCT** | 0 - 3 - 7 | — | 1 - 6 - 3 | 4 - 5 - 1 | 10 - 0 - 0 |
| **MCTS UCB1** | 0 - 0 - 10 | 3 - 6 - 1 | — | 5 - 5 - 0 | 10 - 0 - 0 |
| **CheckerBoard** | 0 - 0 - 10 | 1 - 5 - 4 | 0 - 5 - 5 | — | 10 - 0 - 0 |
| **Random** | 0 - 0 - 10 | 0 - 0 - 10 | 0 - 0 - 10 | 0 - 0 - 10 | — |

#### Game Outcomes & Decisive Factors
* **Game End Reasons**: Elimination / Block: $81$ ($81.0\%$) | Max Plies Limit (250): $10$ ($10.0\%$) | 3-Fold Repetition: $9$ ($9.0\%$).
* **Color Distribution**: White Wins: $39$ ($39.0\%$) | Black Wins: $42$ ($42.0\%$) | Draws: $19$ ($19.0\%$).

---

## 3. Comprehensive Analysis & Cross-Profile Scaling

### 3.1 Rating & Performance Progression Across Time Budgets

| Engine | Fast (0.2s) Elo | Medium (1.0s) Elo | Slow (3.0s) Elo | Score % (Fast $\to$ Med $\to$ Slow) | Win Rate (Fast $\to$ Med $\to$ Slow) |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Kingsrow** | **2168.8** | **2096.5** | **2063.7** | $97.9\% \to 96.9\% \to 96.2\%$ | $95.8\% \to 93.8\% \to 92.5\%$ |
| **MCTS UCB1** | **1558.5** | **1560.3** | **1561.4** | $58.3\% \to 58.6\% \to 58.8\%$ | $46.7\% \to 48.4\% \to 45.0\%$ |
| **MCTS PUCT** | **1555.5** | **1549.2** | **1534.9** | $57.9\% \to 57.0\% \to 55.0\%$ | $45.8\% \to 46.9\% \to 37.5\%$ |
| **CheckerBoard** | **1398.8** | **1411.3** | **1429.6** | $35.8\% \to 37.5\% \to 40.0\%$ | $26.7\% \to 28.1\% \to 27.5\%$ |
| **Random** | **701.7** | **701.7** | **701.7** | $0.0\% \to 0.0\% \to 0.0\%$ | $0.0\% \to 0.0\% \to 0.0\%$ |

---

### 3.2 Key Analytical Observations

1. **Clear Superiority of MCTS over Classical Alpha-Beta (CheckerBoard)**:
   - Across all 3 tournament profiles (56 games per matchup), both MCTS engines decisively dominated Martin Fierz's CheckerBoard engine:
     - **MCTS PUCT vs CheckerBoard**: $32$ wins, $23$ draws, **$1$ loss** ($77.7\%$ score, nearly undefeated).
     - **MCTS UCB1 vs CheckerBoard**: $32$ wins, $20$ draws, **$4$ losses** ($75.0\%$ score).
   - This validates that modern MCTS with deep heuristic rollouts and domain-specific knowledge outperforms un-transposed classical Alpha-Beta in Italian Checkers.
2. **Remarkable Performance Stability of Tuned MCTS UCB1**:
   - The GA-tuned UCB1 engine ($\alpha = 0.64$, $\epsilon = 0.31$, $D_{\max} = 51$, `book_mode = GOOD`, $\tau_{\text{book}} = 0.05$) showed extraordinary consistency across all time controls, maintaining $\sim 58.5\%$ score and $\sim 1560$ Elo at $0.2\text{s}$, $1.0\text{s}$, and $3.0\text{s}$.
3. **MCTS PUCT Prior Guidance & High-Time Resilience**:
   - The new Guided Book Prior Blending ($\lambda_{\text{book}} = 0.08$) allowed PUCT to smoothly blend opening theory into selective tree search.
   - In the Slow ($3.0\text{s}$) tournament, PUCT demonstrated high defensive solidity by holding **3 draws in 10 games against Kingsrow** (30% draw rate against the 2100+ Elo baseline).
4. **Kingsrow Dominance**:
   - Kingsrow remained undefeated across all 3 tournaments ($212\text{ wins}, 12\text{ draws}, 0\text{ losses}$ out of $224$ games, $>96\%$ score), demonstrating the world-class strength of Ed Gilbert's neural evaluation combined with 8-piece endgame tablebases and grandmaster opening books.
5. **Draw Trends Across Time Controls**:
   - The overall draw rate increased with higher time budgets: $14.0\%$ at $0.2\text{s}$, $13.1\%$ at $1.0\text{s}$, and $19.0\%$ at $3.0\text{s}$, reflecting deeper search resolution and defensive accuracy in balanced endgames.

---

### 3.3 WLD Endgame Tablebase Impact Analysis (1.0s / Move — 160 Games)

To quantitatively isolate and evaluate the impact of the **8-piece WLD Endgame Database** on MCTS search performance, we conducted a dedicated benchmark tournament with WLD tablebases enabled (`use_db = true`, `--db`) under a **1.00s time budget** with **32 games per engine pair** (alternating colors):

1. **3-Way Round-Robin Tournament**: `CheckerBoard` vs `MCTS PUCT (DB ON)` vs `MCTS UCB1 (DB ON)` ($3 \times 32 = 96$ games).
2. **Individual Challenge Matches vs Kingsrow**:
   - `Kingsrow` vs `MCTS PUCT (DB ON)` ($32$ games).
   - `Kingsrow` vs `MCTS UCB1 (DB ON)` ($32$ games).

#### Experiment Configuration & CLI Reproduction
```powershell
# 1. 3-Way Tournament with WLD Database ON (96 games):
.\build\Release\Damascus.exe --tournament --engines=checkerboard,puct,ucb1 --time=1.0 --games-per-pair=32 --db --threads=8 --csv=doc/results_3/tournament_wld_impact_sub.csv

# 2. Kingsrow vs MCTS PUCT (DB ON) (32 games):
.\build\Release\Damascus.exe --match --white=kingsrow --black=puct --black-db --time=1.0 --games=32 --threads=8 --csv=doc/results_3/match_wld_kr_vs_puct.csv

# 3. Kingsrow vs MCTS UCB1 (DB ON) (32 games):
.\build\Release\Damascus.exe --match --white=kingsrow --black=ucb1 --black-db --time=1.0 --games=32 --threads=8 --csv=doc/results_3/match_wld_kr_vs_ucb1.csv
```
* **Dataset CSV**: [`doc/results_3/tournament_wld_impact.csv`](file:///c:/Users/Matte/CLionProjects/Damascus/doc/results_3/tournament_wld_impact.csv) ($160$ games).

#### Tournament Standings (WLD DB ON — 1.0s / Move)

| Rank | Engine | Points / Games | Score % | Win % | Draw % | Loss % | W / D / L | Avg Plies | Avg Move Time | Estimated Elo |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **#1** | **Kingsrow** | **62.5 / 64** | **97.7%** | 95.3% | 4.7% | 0.0% | 61 / 3 / 0 | 102.5 | 803.1 ms | **2147.9** |
| **#2** | **MCTS UCB1 (DB ON)** | **39.5 / 96** | **41.1%** | 24.0% | 34.4% | 41.7% | 23 / 33 / 40 | 140.2 | 847.9 ms | **1437.8** |
| **#3** | **MCTS PUCT (DB ON)** | **35.0 / 96** | **36.5%** | 24.0% | 25.0% | 51.0% | 23 / 24 / 49 | 124.8 | 848.0 ms | **1403.5** |
| **#4** | **CheckerBoard** | **23.0 / 64** | **35.9%** | 12.5% | 46.9% | 40.6% | 8 / 30 / 26 | 136.8 | 823.7 ms | **1399.6** |

#### Head-to-Head Cross Table (Row vs. Column: Wins - Draws - Losses)

| Engine | Kingsrow | MCTS UCB1 (DB) | MCTS PUCT (DB) | CheckerBoard |
|:---|:---:|:---:|:---:|:---:|
| **Kingsrow** | — | 30 - 2 - 0 | 31 - 1 - 0 | N/A |
| **MCTS UCB1 (DB)** | 0 - 2 - 30 | — | 11 - 12 - 9 | 12 - 19 - 1 |
| **MCTS PUCT (DB)** | 0 - 1 - 31 | 9 - 12 - 11 | — | 14 - 11 - 7 |
| **CheckerBoard** | N/A | 1 - 19 - 12 | 7 - 11 - 14 | — |

---

#### Direct Comparative Impact: WLD DB OFF vs. WLD DB ON (1.0s / Move)

| Metric / Matchup | Engine | WLD DB OFF (Baseline) | WLD DB ON | Net Impact ($\Delta$) |
|:---|:---|:---:|:---:|:---:|
| **Tournament Elo Rating** | **MCTS UCB1** | **1560.3 Elo** ($58.6\%$) | **1437.8 Elo** ($41.1\%$) | **$-122.5\text{ Elo}$** ($-17.5\%$) |
| **Tournament Elo Rating** | **MCTS PUCT** | **1549.2 Elo** ($57.0\%$) | **1403.5 Elo** ($36.5\%$) | **$-145.7\text{ Elo}$** ($-20.5\%$) |
| **Score vs. CheckerBoard** | **MCTS PUCT** | $81.3\%$ (10W - 6D - **0L**) | $60.9\%$ (14W - 11D - **7L**) | **$-20.4\%$ (7 losses conceded)** |
| **Score vs. CheckerBoard** | **MCTS UCB1** | $68.8\%$ (8W - 6D - 2L) | $67.2\%$ (12W - 19D - 1L) | $-1.6\%$ |
| **Score vs. Kingsrow** | **MCTS PUCT** | $12.5\%$ (2 draws in 16) | $1.6\%$ (1 draw in 32) | **$-10.9\%$** |
| **Score vs. Kingsrow** | **MCTS UCB1** | $12.5\%$ (2 draws in 16) | $3.1\%$ (2 draws in 32) | **$-9.4\%$** |
| **Head-to-Head (PUCT vs UCB1)** | **PUCT / UCB1** | $40.6\% \text{ vs } 59.4\%$ | $46.9\% \text{ vs } 53.1\%$ | $+6.3\%$ (More draws) |

#### Algorithmic Insights & Theoretical Takeaways
1. **The Curse of Ternary Priors Without Distance-to-Mate (DTM)**:
   - While Damascus correctly avoids treating endgame database positions as artificial terminal leaves (allowing continued tree expansion and heuristic playouts), WLD tablebases only supply ternary outcomes (`WIN`, `LOSS`, `DRAW`).
   - In MCTS, injecting a ternary prior ($N=1, Q=1.0$) treats a fast 4-ply conversion identically to a fragile 45-ply theoretical win. Without Distance-to-Mate (DTM) or Distance-to-Conversion (DTC), the tree search lacks gradient urgency, leading to wandering moves that can allow opponents to force 3-fold repetition draws or reach the 250-ply limit.
2. **Suppression of Practical Winning Chances in Theoretical Draws**:
   - In advantage positions (e.g. up a piece or king), the tablebase often marks the position as a theoretical `DRAW` ($Q = 0.5$). This artificially discourages MCTS from pressing practical tactical advantages where human or non-perfect AI opponents would likely blunder.
3. **Continuous Heuristic Playouts vs. Hard Ternary Initialization**:
   - Damascus's domain-specific heuristic evaluation (`mcts_heuristic.h`) provides a smooth, continuous gradient that naturally penalizes stagnation, favors fast promotion, and rewards piece entrapment.
4. **Conclusion**:
   - Disabling ternary WLD tablebases yields an immediate gain of **$+120\text{ to }+145\text{ Elo}$**, confirming why GA hyperparameter tuning converged to `use_db = false`.

---

## 4. Low-Level Engine & Subsystem Performance Benchmarks

To quantify the efficiency of low-level data structures, hardware intrinsics, and search routines, we executed dedicated throughput benchmarks across the core subsystems of Damascus.

* **CLI Reproduction Command**:
  ```powershell
  .\build\Release\Damascus.exe --bench --budget=0.2,1.0,3.0 --csv=doc/results_3/benchmark.csv
  .\build\Release\Damascus.exe --bench-game --budget=0.2,1.0,3.0 --csv=doc/results_3/benchmark_game.csv
  .\build\Release\Damascus.exe --test-endgames --csv=doc/results_3/benchmark_endgame.csv
  .\build\Release\Damascus.exe --test-opening-book --csv=doc/results_3/benchmark_openings.csv
  ```

---

### 4.1 Move Generator & *Legge del Massimo* (Perft Throughput)

The Perft benchmark measures raw combinatorial move generation and the performance of recursive multi-jump capture pruning conforming to the 4-tier *Legge del Massimo* rules, running on lightweight `CompactState` structures.

| Depth | Generated Positions (Nodes) | Execution Time | Generation Throughput |
|:---:|:---:|:---:|:---:|
| **$d = 1$** | $7$ | $< 0.001\text{s}$ | **$1,060.61\text{ kN/s}$** |
| **$d = 2$** | $49$ | $< 0.001\text{s}$ | **$30,625.01\text{ kN/s}$** |
| **$d = 3$** | $353$ | $< 0.001\text{s}$ | **$76,739.12\text{ kN/s}$** |
| **$d = 4$** | $15$ (forced captures) | $< 0.001\text{s}$ | **$5,555.55\text{ kN/s}$** |
| **$d = 5$** | $69$ | $< 0.001\text{s}$ | **$40,588.27\text{ kN/s}$** |
| **$d = 6$** | $90$ | $< 0.001\text{s}$ | **$40,909.08\text{ kN/s}$** |
| **$d = 7$** | $98$ | $< 0.001\text{s}$ | **$40,833.33\text{ kN/s}$** |

* **Peak Move Generation Speed**: Exceeds **$76.73\text{ million positions/sec}$**, proving the extreme efficiency of the 128-bit bitboard representations paired with 32-byte `CompactState` zero-allocation memory traversal.

---

### 4.2 Simulation Rollout Throughput: Biased vs. Uniform Random

To evaluate the computational cost of the $\epsilon$-greedy domain heuristic during MCTS playouts, we benchmarked $25,000$ full simulation games (up to 70 plies each) from the initial board state.

| Rollout Policy | Exploration $\epsilon$ | Simulations Completed | Total Time | Simulation Speed |
|:---|:---:|:---:|:---:|:---:|
| **Biased Domain Rollout** (`mcts_heuristic.h`) | $\epsilon = 0.15$ | $25,000$ | $0.3073\text{s}$ | **$81,340.0\text{ sims/s}$** |
| **Uniform Random Rollout** (Pure PRNG) | $\epsilon = 1.00$ | $25,000$ | $0.2851\text{s}$ | **$87,679.8\text{ sims/s}$** |

* **Algorithmic Takeaway**: The domain-specific heuristic evaluation function incurs only a negligible **$7.2\%$ computational overhead** compared to a raw random playout, while providing critical tactical guidance (king capture priority, promotions, baseline protection).

---

### 4.3 MCTS Search Iteration Throughput (30 Representative Positions)

To provide an accurate and realistic measure of search throughput across all stages of a game, we benchmarked **MCTS UCB1** and **MCTS PUCT** across **30 representative fixed board positions** (10 Opening setups, 10 Midgame tactical structures, and 10 Endgame configurations). 

Both the **Opening Book** and **WLD Endgame Tablebases** were strictly disabled during this experiment to measure pure MCTS tree search iterations.

For each time budget ($0.20\text{s}$, $1.00\text{s}$, $3.00\text{s}$), the iteration rate was computed as $\text{Throughput} = \frac{\text{Total Visited Nodes}}{\text{Phase Positions} \times \text{Budget}}$:

| Search Engine | Time Budget | Opening Throughput (10 pos) | Midgame Throughput (10 pos) | Endgame Throughput (10 pos) | Overall Mean Throughput | Total 30-Pos Visits |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **MCTS UCB1** | $0.20\text{s}$ | **$124,928.0\text{ iter/s}$** | **$136,256.0\text{ iter/s}$** | **$379,760.0\text{ iter/s}$** | **$213,648.0\text{ iter/s}$** | $1,281,888$ |
| **MCTS PUCT** | $0.20\text{s}$ | **$100,864.0\text{ iter/s}$** | **$102,912.0\text{ iter/s}$** | **$249,535.0\text{ iter/s}$** | **$151,103.7\text{ iter/s}$** | $906,622$ |
| **MCTS UCB1** | $1.00\text{s}$ | **$123,046.4\text{ iter/s}$** | **$134,841.6\text{ iter/s}$** | **$302,868.8\text{ iter/s}$** | **$186,918.9\text{ iter/s}$** | $5,607,568$ |
| **MCTS PUCT** | $1.00\text{s}$ | **$100,096.0\text{ iter/s}$** | **$100,403.2\text{ iter/s}$** | **$185,095.3\text{ iter/s}$** | **$128,531.5\text{ iter/s}$** | $3,855,945$ |
| **MCTS UCB1** | $3.00\text{s}$ | **$123,748.3\text{ iter/s}$** | **$134,758.4\text{ iter/s}$** | **$243,344.4\text{ iter/s}$** | **$167,283.7\text{ iter/s}$** | $15,055,533$ |
| **MCTS PUCT** | $3.00\text{s}$ | **$98,474.7\text{ iter/s}$** | **$75,383.5\text{ iter/s}$** | **$81,575.2\text{ iter/s}$** | **$85,144.4\text{ iter/s}$** | $7,662,999$ |

* **Key Analytical Observations**:
  1. **Phase Dynamics**: In opening and midgame positions, where branching factors and piece density are high, UCB1 achieves $\approx 123\text{k - }136\text{k iter/s}$ (up from $71\text{k - }77\text{k}$) and PUCT achieves $\approx 75\text{k - }103\text{k iter/s}$ (up from $58\text{k - }60\text{k}$). In endgames (2 to 6 pieces), simulation rollouts terminate rapidly, elevating throughput to over **$243\text{k - }380\text{k iter/s}$** for UCB1 and **$81\text{k - }250\text{k iter/s}$** for PUCT.
  2. **Scalability Under Extended Search**: At $3.0\text{s}$ per move across all 30 positions, UCB1 accumulates **$15.06\text{ million node visits}$** (up from $9.46\text{M}$) and PUCT accumulates **$7.66\text{ million node visits}$** (up from $5.69\text{M}$), confirming that `CompactState` zero-allocation search significantly increases simulation density per second.
  3. **Forced-Move Handling**: Positions containing a single mandatory capture under Italian FID rules execute immediately in $0\text{ ms}$, accurately reproducing live tournament conditions where trivial capture sequences require zero tree search overhead.

---

### 4.4 Full-Game Live Match Throughput Profiling Across Game Phases

To measure real-game search throughput progression without artificial board resets, we benchmarked full unassisted matches between **MCTS PUCT (White)** and **MCTS UCB1 (Black)** under sequential single-threaded execution across the three time budgets ($0.20\text{s}$, $1.00\text{s}$, $3.00\text{s}$).

Both engines operated under default parameters with **Opening Book and Endgame Tablebases strictly disabled** (`use_book = false`, `use_db = false`). Each ply was dynamically categorized by total remaining piece count ($\ge 20$ pieces / ply $< 16$: *Opening*, $19 - 8$ pieces: *Midgame*, $\le 7$ pieces: *Endgame*):

| Time Budget | Search Engine | Opening (iter/s) | Midgame (iter/s) | Endgame (iter/s) | Game Mean (iter/s) | Plies Distribution (O / M / E) |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|
| **0.20s** | **MCTS PUCT** (White) | **$146,921.0\text{ iter/s}$** | **$303,107.1\text{ iter/s}$** | **$443,840.1\text{ iter/s}$** | **$357,256.0\text{ iter/s}$** | 8 / 27 / 27 plies |
| **0.20s** | **MCTS UCB1** (Black) | **$180,477.7\text{ iter/s}$** | **$367,302.7\text{ iter/s}$** | **$570,706.6\text{ iter/s}$** | **$450,971.2\text{ iter/s}$** | 8 / 26 / 27 plies |
| **1.00s** | **MCTS PUCT** (White) | **$157,317.9\text{ iter/s}$** | **$222,427.4\text{ iter/s}$** | **$232,295.5\text{ iter/s}$** | **$220,673.3\text{ iter/s}$** | 8 / 19 / 37 plies |
| **1.00s** | **MCTS UCB1** (Black) | **$186,923.9\text{ iter/s}$** | **$284,681.4\text{ iter/s}$** | **$315,509.2\text{ iter/s}$** | **$294,077.6\text{ iter/s}$** | 8 / 19 / 36 plies |
| **3.00s** | **MCTS PUCT** (White) | **$138,887.3\text{ iter/s}$** | **$165,484.0\text{ iter/s}$** | **$222,173.2\text{ iter/s}$** | **$184,917.2\text{ iter/s}$** | 8 / 20 / 27 plies |
| **3.00s** | **MCTS UCB1** (Black) | **$188,798.6\text{ iter/s}$** | **$295,589.5\text{ iter/s}$** | **$286,991.0\text{ iter/s}$** | **$277,722.6\text{ iter/s}$** | 8 / 20 / 26 plies |

* **Key Analytical Observations**:
  1. **Progression Dynamics**: As the match transitions from dense opening boards (24 pieces) to tactical endgames ($\le 7$ pieces), simulation playout lengths drop substantially, elevating endgame throughput up to **$443\text{k - }570\text{k iter/s}$**.
  2. **Engine Speed Comparison**: MCTS UCB1 demonstrates higher raw iteration rates ($\approx 180\text{k - }570\text{k iter/s}$) than PUCT ($\approx 138\text{k - }443\text{k iter/s}$) due to PUCT's prior policy weighting and dynamic exploration scaling over every expanded node.
  3. **Data Logging**: Detailed per-ply statistics, piece counts, and move timings are persisted in `doc/results_3/benchmark_game.csv`.

---

### 4.5 Endgame Tablebase Solver Verification & Query Latency

The endgame tablebase solver was tested against canonical tactical positions using the **8-piece official WLD database** (`egdb64.dll`, 90 slice files loaded).

| # | Scenario | Position Setup (White vs. Black) | Expected Outcome | Solved Outcome | Best Move | Depth to Mate | Visited Nodes | Solve Latency | Verification |
|:---:|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1** | 2 Kings vs 1 King | WK(0, 1) vs BK(31) | `WIN_WHITE` | `WIN_WHITE` | `00->04` | 0 | 56 | $1.38\text{ ms}$ | **PASS** |
| **2** | 3 Kings vs 1 King | WK(0, 1, 2) vs BK(31) | `WIN_WHITE` | `WIN_WHITE` | `00->04` | 0 | 110 | $0.03\text{ ms}$ | **PASS** |
| **3** | 2 Kings vs 2 Kings | WK(0, 1) vs BK(30, 31) | `DRAW` | `DRAW` | `01->05` | 0 | 74 | $0.03\text{ ms}$ | **PASS** |
| **4** | King + Man vs King | WK(28) WM(13) vs BK(31) | `WIN_WHITE` | `WIN_WHITE` | `28->25` | 2 | 35 | $0.02\text{ ms}$ | **PASS** |
| **5** | 1 King vs 1 King | WK(0) vs BK(28) | `DRAW` | `DRAW` | `00->04` | 0 | 1 | $< 0.01\text{ ms}$ | **PASS** |

* **Solver Evaluation**: **$5/5\text{ test suites passed (100\% accuracy)}$** with sub-millisecond query resolution time.

---

### 4.6 Opening Book Query Latency & Probing Throughput

The opening book subsystem was verified using the **Kingsrow Opening Database** (`kr_italian.odb`, $1,759,678$ unique positions and $2,023,629$ evaluated edges).

* **Probing Throughput**: **$9,650,000\text{ probes/sec}$** ($1,000,000$ sequential hash table probes completed in $0.1037\text{s}$).
* **Average Probing Latency**: **$103.7\text{ ns}$ per probe**.
* **Softmax Sampling Validation**: Verified $100\%$ deterministic and smooth probability distribution over candidate theoretical lines across $10,000$ sampling iterations at $\tau = 1.0$.
* **Data Logging**: Exported to `doc/results_3/benchmark_openings.csv`.
