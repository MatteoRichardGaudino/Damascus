# Project Specification: MCTS-Based Italian Draughts Artificial Player

> **Source**: `doc/progettoIW.pdf`  
> **Topic**: Monte Carlo Tree Search (MCTS) in Game Systems & Project Requirements for Italian Draughts (*Dama Italiana*)  
> **Language**: English (translated from Italian)

---

## 1. Monte Carlo Tree Search (MCTS) Overview

In the context of decision trees and turn-based games, **Monte Carlo Tree Search (MCTS)** relies on random (or semi-random) simulations and an **exploration vs. exploitation** trade-off mechanism instead of (or as a support to) classical heuristic evaluation functions.

### Key Characteristics
- **Asymmetric Best-First Search**: Selectively deepens promising branches rather than exploring uniformly.
- **Anytime Algorithm**: Can be interrupted at any moment to return the best action found so far; search quality improves with available computation time.
- **Domain-Independent**: Requires only the forward game rules (state transition and terminal check), though domain-specific knowledge can be injected into rollouts or priors.

### The 4 Core Phases of MCTS
MCTS iteratively executes four sequential phases:
1. **Selection**
2. **Expansion**
3. **Simulation (Rollout)**
4. **Backpropagation**

```
 [Selection] ---> [Expansion] ---> [Simulation] ---> [Backpropagation]
      ^                                                     |
      |_____________________________________________________|
```

---

## 2. Detailed MCTS Phases

### 2.1 Selection (Exploration vs. Exploitation)
- **Goal**: Traverse the already constructed portion of the tree from the root to identify the most urgent search "frontier" node to expand.
- **Problem Formulation**: At each visited node (representing a board state/configuration), the algorithm solves a **Best Arm Identification (BAI)** / Multi-Armed Bandit problem.
- **Selection Policy (UCB1)**: Since MCTS is an anytime algorithm without a fixed a priori sample budget, the standard technique is **UCB1 (Upper Confidence Bound 1)**:

$$\text{UCB}(i) = \hat{\mu}_i + \alpha \sqrt{\frac{2 \ln(N_j)}{N_i}}$$

Where:
- $j$: Parent node.
- $i$: Child node under evaluation.
- $\hat{\mu}_i = \frac{w_i}{N_i}$: Empirical average reward of child node $i$ ($w_i$ is accumulated reward, $N_i$ is number of times child $i$ was visited).
- $N_j$: Total number of times the parent node $j$ has been visited ($N_j = \sum_k N_k$).
- $N_i$: Number of times child node $i$ has been visited.
- $\alpha$: Exploration coefficient / hyperparameter balancing exploitation ($\hat{\mu}_i$) vs. exploration.

---

### 2.2 Expansion
- **Trigger**: When the selection phase reaches a leaf node or a node that is not fully expanded.
- **Goal**: Decide how to grow the tree by turning a potential legal move into a concrete tree node.
- **Steps**:
  1. Generate all legal moves from the current node state.
  2. Exclude moves that are already instantiated as children.
  3. Select one of the remaining unvisited moves.
  4. Instantiate a new `Node` object representing that action/state and attach it to the parent.

---

### 2.3 Simulation (Rollout)
- **Goal**: Evaluate a newly created node ($N=0$) that has never been visited before, since its true value is unknown.
- **Mechanism**: Perform an empirical simulation (*rollout*) playing out a game from the new node state until a terminal condition or cut-off depth is reached.
- **Policy**: Moves during simulation are chosen using a fast, lightweight policy (typically random or semi-random/heuristic).
- **Optimization & Memory Rules**:
  - **Do NOT create tree nodes** during rollout to conserve memory and maintain high throughput.
  - Apply game rules and state transitions rapidly in sequence.
- **Edge-case Handling**:
  - Must explicitly handle draws/ties, stalemates, determined/won endgames, and inconclusive repetitive loops.

---

### 2.4 Backpropagation
- **Goal**: Propagate the simulation outcome / reward $R$ backwards from the newly expanded leaf node up to the root across all ancestor nodes.
- **Update Rule** (for each ancestor $j$ along the traversed path):
  $$w_j \leftarrow w_j + R$$
  $$N_j \leftarrow N_j + 1$$

---

## 3. Project Requirements (*Specifiche di Progetto*)

### 3.1 Objective & Technology Constraints
- **Domain**: Artificial Player (AP) for **Italian Draughts** (*Dama Italiana*).
- **Programming Language**: **C only**. No other programming language is permitted.
- **User Interface (GUI)**: Interactive graphical application where a human player can challenge the AP by dragging/clicking and moving pieces on a draughts board using the mouse.

---

### 3.2 AI & Algorithmic Requirements

#### 1. Exploration vs. Exploitation Models
The engine must implement at least **two** selection/exploration models:
1. **UCB1**: Classic model as described above.
2. **Alternative Model**: Another variant such as UCB variants or **PUCT** (AlphaGo-style Predictor Upper Confidence Tree).

#### 2. Anytime Search Budgets
Both models must support **three configurable thinking time limits**:
- **Fast**: `0.2 seconds` per move
- **Medium**: `1.0 second` per move
- **Slow**: `3.0 seconds` per move

#### 3. Playing Performance
- Playing strength and competitive performance are fundamental grading criteria. Poorly performing APs will be penalized.

---

### 3.3 Performance & Memory Optimization Guidelines
Because MCTS performance directly correlates with the number of simulated playouts per second:

1. **Bitboard Representation**:
   - Use bitboards with precomputed bitmasks for ultra-fast bitwise move generation and state updates.
2. **Zero Allocation in Rollouts**:
   - Do **NOT** clone state structures during rollout. Use in-place state mutation (make/unmake) or lightweight scratch buffers.
3. **Lookup Tables (LUTs)**:
   - Use lookup tables to accelerate legal move identification, capture checks, and king promotions.
4. **Smart Rollouts / Non-random Endgames**:
   - Completely uniform random rollouts in Draughts lead to excessive draws or indecisive loops.
   - Inject Italian Draughts rules/heuristics or known endgame patterns to quickly steer games toward decisive conclusions.

---

### 3.4 Tuning & Experimental Analysis

1. **Principled Hyperparameter Tuning**:
   - Critical hyperparameters (such as exploration parameter $\alpha$ in Selection, rollout heuristics, etc.) must be tuned using principled methods:
     - Genetic Algorithms (GA)
     - Round-Robin Tournaments
     - Best Arm Identification (BAI)
     - CLOP (Confidence Local Optimization, AlphaGo style) or combinations thereof.
   - Tuning stages typically utilize low time limits (e.g., `0.2s` per move) to enable thousands of trial games.
2. **Experimental Evaluation & Report**:
   - Deliver an adequate experimental report quantifying and comparing the win rates, rollout throughput, and performance of different AP configurations, time budgets, and selection policies.

---

## 4. Quick Summary Checklist for Implementation

| Category | Requirement | Details |
| :--- | :--- | :--- |
| **Language** | C (Pure C) | Strict requirement; no C++ / Python for core engine |
| **Game** | Italian Draughts (*Dama Italiana*) | Strict rule adherence (forced captures, highest value capture, king privileges) |
| **UI** | Mouse-driven GUI | Human vs. AP playable interface |
| **Search Models** | 2 Selection Models | UCB1 + Alternative (e.g. PUCT / UCB variant) |
| **Time Limits** | 3 Anytime Profiles | 0.2s, 1.0s, 3.0s |
| **State Rep.** | Bitboards | Fast bitwise logic, precomputed masks, LUTs |
| **Rollout Speed** | No allocation / no cloning | In-place or scratch buffer simulation, smart endgames |
| **Tuning** | Principled Hyperparameter Tuning | Automated tuning (CLOP, GA, Round-Robin) |
| **Evaluation** | Experimental Study | Detailed benchmarks and empirical comparisons |
