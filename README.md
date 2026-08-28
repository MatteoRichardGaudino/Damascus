# Damascus ♟️

[![Language](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Windows%20%28Recommended%29%20%7C%20macOS%20%7C%20Linux-brightgreen.svg)]()
[![Graphics](https://img.shields.io/badge/Graphics-OpenGL%203.3%20Core-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)]()

**Damascus** is a high-performance modular platform and 3D engine suite for **Italian Checkers** (*Dama Italiana*), developed by **Matteo Richard Gaudino** and **Claudio Velotti** for the *Intelligent Web* course (Academic Year 2025/2026).

Written entirely in **pure C (C11)**, Damascus combines a modern 3D OpenGL desktop interface with an advanced artificial intelligence laboratory. It features custom Monte Carlo Tree Search engines, integration with world-class third-party engines, exact 8-piece endgame tablebases, opening books, and a comprehensive multithreaded CLI benchmarking and genetic algorithm tuning framework.

---

## 🧠 AI Engines

Damascus supports multiple AI engines through a unified stateful interface:

### 1. Custom MCTS UCB1 *(Developed by us)*
Custom Monte Carlo Tree Search engine based on the UCB1 (Upper Confidence Bound 1) algorithm.  
**Implementation Source Files**:
- Header: [`src/mcts_ucb1.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_ucb1.h)
- Source: [`src/mcts_ucb1.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_ucb1.c)

### 2. Custom MCTS PUCT *(Developed by us)*
Custom Monte Carlo Tree Search engine based on the PUCT (Predictor + Upper Confidence Bound for Trees / AlphaZero-style) algorithm.  
**Implementation Source Files**:
- Header: [`src/mcts_puct.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_puct.h)
- Source: [`src/mcts_puct.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/mcts_puct.c)

### 3. CheckerBoard — *Dama Italiana Engine by Martin Fierz*
**CheckerBoard** is a widely recognized universal graphical interface and standard API for checkers engines created by **Martin Fierz**. 
- **Engine Used**: Damascus integrates **Martin Fierz's official Dama Italiana engine** (`dama.c` / `damad.dll`).
- **Algorithm**: Iterative-deepening Alpha-Beta search with quiescence capture search, material evaluation, center control, back-rank preservation, and strict enforcement of the Italian Checkers *Law of Maximum*.
- **Implementation Adapter**: [`src/engine_checkerboard.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_checkerboard.h) and [`src/engine_checkerboard.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_checkerboard.c).

### 4. Kingsrow Italian — *by Ed Gilbert*
**Kingsrow Italian** is widely regarded as the **strongest Italian Checkers engine in the world**.
- **Architecture**: Closed-source, highly optimized Windows binary using neural network evaluation weights, deep alpha-beta search, and parallel processing.
- **Integration**: Integrated into Damascus via an IPC bridge (`kr_bridge.exe` and `kr_italian64.dll`).
- **Features**: Seamlessly interfaces with Kingsrow's full 8-piece Endgame Tablebases (WLD) and massive opening database (`kr_italian.odb`).
- **Implementation Adapter**: [`src/engine_kingsrow.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_kingsrow.h) and [`src/engine_kingsrow.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_kingsrow.c).

### 5. Random Baseline Engine
A uniform stochastic move generator used for unit testing, baseline rating calibration, and genetic algorithm fitness benchmarks.  
**Implementation Source Files**: [`src/engine_random.h`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_random.h) and [`src/engine_random.c`](file:///c:/Users/Matte/CLionProjects/Damascus/src/engine_random.c).

---

## 🇮🇹 Italian Checkers Rules Summary

Damascus strictly enforces the official rules of the Italian Draughts Federation (*FID - Federazione Italiana Dama*):

1. **Board Orientation**: Played on an 8×8 board using only the **32 dark squares**, oriented so that the bottom-right corner square is **dark**.
2. **Pieces**: White moves first. Each side starts with 12 men (*pedine*).
3. **Pawn Movement**: Men move diagonally forward by one square.
4. **Promotion**: Reaching the opponent's back rank promotes a man to a King (*Dama*).
5. **King Movement**: Kings move diagonally forward and backward by one square.
6. **Capture Obligations & The Law of Maximum (*Legge del Massimo*)**:
   - Captures are **mandatory**.
   - Pawns **cannot capture Kings**; Kings can capture both Pawns and Kings.
   - Pawns capture only diagonally forward; Kings capture forward and backward.
   - **Priority 1 (Quantity)**: You must choose the path that captures the **largest total number of pieces**.
   - **Priority 2 (Piece Quality)**: If equal piece count, a **King must make the capture** over a Pawn.
   - **Priority 3 (Captured Quality)**: If equal piece count for a King, you must choose the path that captures the **most Kings**.
   - **Priority 4 (First Encounter)**: If all above are equal, the King must choose the path that captures a **King first**.

---

## ⚙️ Compilation & Build Instructions

> [!IMPORTANT]
> **Recommended OS**: **Windows (x64)** using **Microsoft Visual Studio (MSVC)**.  
> Kingsrow (`kr_italian64.dll` / `kr_bridge.exe`) and the official 8-piece Endgame Database driver (`egdb64.dll`) are closed-source native Windows binaries. Compiling on Windows provides full access to all engines, databases, and graphical features out of the box.

### Prerequisites
- **Operating System**: Windows 10/11 (64-bit).
- **Compiler**: Microsoft Visual Studio 2019/2022 (MSVC with C11 support) or Clang/GCC on Windows.
- **Build System**: [CMake](https://cmake.org/download/) 3.16 or higher.
- **Git**: For automated fetching of dependencies (GLFW, cglm).

### Build with CMake and Visual Studio (MSVC)

Open **PowerShell** or **Command Prompt** in the project root directory:

```powershell
# 1. Generate build configuration for Visual Studio x64
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. Build the project in Release mode
cmake --build build --config Release
```

The compiled binary `Damascus.exe` and companion test executables will be generated in `build\Release\`.

*(Note: Damascus can also be built on macOS and Linux for its custom MCTS and Random engines using standard CMake tools, with Kingsrow bridge support requiring Wine).*

---

## 📁 Directory Structure & Database Setup

To run Damascus with full opening book and 8-piece endgame tablebase capabilities, verify that the application and database files are organized as follows:

```
Damascus/
├── build/
│   └── Release/
│       ├── Damascus.exe                    # Main executable
│       └── shaders/                        # Auto-copied on build
│           ├── basic.vert
│           └── basic.frag
├── data/
│   └── wld/                                # Official 8-Piece WLD Database
│       ├── db2.cpr1 ... db2.idx1
│       ├── db3.cpr1 ... db3.idx1
│       ├── db4.cpr1 ... db4.idx1
│       ├── db5.cpr1 ... db5.idx1
│       ├── db6.cpr1 ... db6.idx1
│       ├── db7.cpr1 ... db7.idx1
│       └── db8-*.cpr1 ... db8-*.idx1       # (90 files total: 45 .cpr1 + 45 .idx1)
├── third_party/
│   └── engines/
│       ├── kingsrow_italian/
│       │   └── app/
│       │       ├── egdb64.dll              # Kingsrow EGDB tablebase driver
│       │       └── engines/
│       │           ├── kr_bridge.exe       # Damascus-Kingsrow IPC bridge
│       │           ├── kr_italian64.dll    # Kingsrow engine DLL
│       │           └── kr_italian.odb      # Kingsrow Opening Book (1.76M positions)
│       └── checkerboard/
│           └── app/
│               └── engines/
│                   └── book.bin            # CheckerBoard opening book
```

### Downloading External Databases

1. **Win/Loss/Draw (WLD) 8-Piece Endgame Database**:
   - **What it is**: Complete 8-piece endgame tablebases generated for Italian Checkers by Ed Gilbert (45 slice pairs, 90 files total).
   - **Download Link**: [Ed Gilbert's Kingsrow Italian Draughts Page](https://edgilbert.org/ItalianCheckers/KingsRowItalian.htm)
   - **Files**: Download the 8-piece WLD database archive (`it_wld_8p.zip` / slice files).
   - **Placement**: Extract all 90 files (`.cpr1` and `.idx1`) into `data/wld/` (or pass a custom path with `--wld-path <path>`).

2. **Opening Book Databases**:
   - **Kingsrow Opening Book (`kr_italian.odb`)** *(Primary / Recommended)*:
     - **What it is**: A high-depth opening book database containing ~1.76 million theoretical positions and ~2.02 million evaluated moves (~33 MB).
     - **Download Link**: Included in the official Kingsrow Italian Checkers installer package from [Ed Gilbert's Kingsrow Italian Draughts Page](https://edgilbert.org/ItalianCheckers/KingsRowItalian.htm) (`kr_italian_setup.exe`).
     - **Placement**: Locate `kr_italian.odb` inside the installed Kingsrow folder and copy it to `third_party/engines/kingsrow_italian/app/engines/kr_italian.odb` (or pass a custom path with `--book-path <path>`).
   - **CheckerBoard Opening Book (`book.bin`)** *(Alternative / Supported)*:
     - **What it is**: CheckerBoard standard opening database by Martin Fierz containing ~1.2 million evaluated positions.
     - **Download Link**: Included with the CheckerBoard package available from [Martin Fierz's CheckerBoard Website](http://www.fierz.ch/checkerboard.php).
     - **Placement**: Place `book.bin` in `third_party/engines/checkerboard/app/engines/book.bin`.

3. **Kingsrow Italian Engine & EGDB Driver**:
   - **Download Link**: [Ed Gilbert's Kingsrow Italian Draughts Page](https://edgilbert.org/ItalianCheckers/KingsRowItalian.htm) (`kr_italian_setup.exe`).
   - **Placement**: Copy `kr_italian64.dll`, `kr_bridge.exe`, and `egdb64.dll` to `third_party/engines/kingsrow_italian/app/engines/` and `third_party/engines/kingsrow_italian/app/` as shown in the directory structure.

---

## 🎮 Running Damascus

### 1. Interactive 3D GUI Mode
Launch the interactive graphical interface:
```powershell
.\build\Release\Damascus.exe
```

- **Controls**:
  - **Left Click**: Select and move pieces via 3D Raycasting.
  - **Right Click + Drag**: Rotate board in 3D (Arcball camera).
  - **Scroll Wheel**: Zoom in/out.
  - **UI Overlay**: Configure player types (Human vs CPU, CPU vs CPU, 2 Players), AI search times, opening book modes, and WLD tablebase parameters in real-time.

---

### 2. Headless CLI Modes & Benchmarking

Damascus provides a rich, multi-threaded Command Line Interface (CLI) for running matches, round-robin tournaments, performance benchmarks, and genetic algorithm optimization.

#### General Command Structure

```
Damascus.exe [MODE] [OPTIONS]
```

#### Available Modes

| Flag | Description | Default Settings |
|---|---|---|
| `--match`, `-m` | Runs a head-to-head match between two engines | 10 games, 1.0s/move |
| `--tournament`, `-t` | Runs a Round-Robin tournament across multiple engines | 20 games/pair, 0.2s/move |
| `--bench`, `-b` | Runs MCTS throughput and simulation rate benchmarks | Budgets: 0.2s, 1.0s, 3.0s |
| `--tune`, `--tune-ga` | Runs Genetic Algorithm (GA) hyperparameter optimization | 16 population, 5 generations |
| `--gui` | Launches the interactive 3D graphical desktop interface | GUI Mode |
| `--help`, `-h` | Prints the complete CLI help manual and options | — |

---

#### CLI Options & Parameters Breakdown

##### General Match & Tournament Options
- `--white=<engine>`: Sets the White engine (`ucb1`, `puct`, `checkerboard`, `kingsrow`, `random`). *Default: `ucb1`*.
- `--black=<engine>`: Sets the Black engine (`ucb1`, `puct`, `checkerboard`, `kingsrow`, `random`). *Default: `puct`*.
- `--engines=<list>`: Comma-separated list of engines for a tournament, or `all` (e.g. `--engines=ucb1,puct,checkerboard,random`).
- `--time=<sec>`, `-T <sec>`: Search time budget per move in seconds. *Default: `1.0` for match, `0.2` for tournament/tuning*.
- `--white-time=<sec>` / `--black-time=<sec>`: Sets asymmetric search times for White or Black.
- `--games=<N>`, `-n <N>`: Total number of games to play in match mode. *Default: `10`*.
- `--games-per-pair=<N>`: Number of games per engine pair in tournament mode. *Default: `20`*.
- `--threads=<N>`, `-j <N>`: Number of concurrent worker threads, or `auto` for all CPU cores. *Default: `1`*.
- `--max-plies=<N>`: Maximum plies allowed per game before declaring a theoretical draw. *Default: `250`*.
- `--opening-plies=<N>`: Number of random initial half-moves played for game diversity. *Default: `2`*.
- `--csv=<path>`, `-o <path>`: Exports structured game logs and tournament summary tables to a CSV file.
- `--quiet`, `-q`: Suppresses the real-time ANSI progress dashboard on `stderr`.
- `--verbose`, `-v`: Prints detailed move-by-move notation and timestamps.

##### Opening Book Options
- `--book-backend=<type>`: Selects opening book backend: `odb` (Kingsrow 1.76M), `bin` (CheckerBoard 1.2M), or `none`.
- `--book-mode=<mode>`: Selects sampling mode: `best` (highest eval), `good` (diverse top moves), `all`, `puct_guided`, or `off`.
- `--book-temp=<float>`: Softmax temperature parameter for move sampling. *Default: `1.0`*.
- `--book-path=<path>`: Custom filesystem path to the opening book file (`.odb` or `.bin`).
- `--no-book` / `--book`: Explicitly disables or enables opening book probing.
- `--white-book` / `--white-no-book`: Enables/disables opening book specifically for the White player.
- `--black-book` / `--black-no-book`: Enables/disables opening book specifically for the Black player.

##### Endgame Tablebase (WLD) Options
- `--wld-backend=<type>`: Selects tablebase backend: `official` (8-piece WLD) or `none`.
- `--wld-path=<path>`: Custom filesystem directory containing the `.cpr1` and `.idx1` tablebase files.
- `--no-db` / `--db`: Disables or enables instant WLD tablebase probing.

##### MCTS Hyperparameter Options
- `--alpha=<float>`: Exploration constant $\alpha$ for MCTS UCB1. *Default: `1.414`*.
- `--c-puct=<float>`: Exploration constant $c_{\text{puct}}$ for MCTS PUCT. *Default: `1.5`*.
- `--tau=<float>`: Softmax policy temperature $\tau$ for PUCT move distribution. *Default: `1.0`*.
- `--epsilon=<float>`: Rollout exploration probability $\epsilon$ for biased simulations. *Default: `0.15`*.
- `--rollout-depth=<int>`: Maximum simulation depth limit. *Default: `70`*.

##### Genetic Algorithm (GA) Tuning Options
- `--target=<engine>`: Target engine architecture to tune: `puct` or `ucb1`. *Default: `puct`*.
- `--pop=<N>`, `--population=<N>`: Number of candidate parameter sets per generation. *Default: `16`*.
- `--generations=<N>`, `--gens=<N>`: Number of evolutionary cycles. *Default: `5`*.
- `--mutation-rate=<float>`: Gene mutation probability ($0.0 - 1.0$). *Default: `0.20`*.
- `--mutation-scale=<float>`: Gaussian perturbation variance for mutated genes. *Default: `0.15`*.
- `--crossover-rate=<float>`: Uniform crossover probability ($0.0 - 1.0$). *Default: `0.80`*.
- `--elite-count=<N>`: Number of top-performing individuals preserved directly across generations. *Default: `2`*.
- `--seed=<N>`: Random seed for deterministic evolutionary runs.

---

#### CLI Usage Examples

##### 1. Head-to-Head Match Examples (`--match`)

- **Standard 20-Game Match between PUCT and UCB1 (1.0s/move, 4 threads)**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=puct --black=ucb1 --games=20 --time=1.0 --threads=4 --csv=results/match_puct_ucb1.csv
  ```

- **Asymmetric Time Handicap Match (PUCT 0.25s vs Kingsrow 2.0s)**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=puct --black=kingsrow --white-time=0.25 --black-time=2.0 --games=10 --threads=2
  ```

- **Opening Book Impact Test (PUCT with Book vs PUCT without Book)**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=puct --black=puct --white-book --black-no-book --games=30 --time=0.5 --threads=6 --csv=results/book_study.csv
  ```

- **Fast Verification Match with Verbose Move Logs**:
  ```powershell
  .\build\Release\Damascus.exe --match --white=ucb1 --black=checkerboard --games=2 --time=0.2 --verbose
  ```

---

##### 2. Round-Robin Tournament Examples (`--tournament`)

- **Full Engine Tournament (PUCT, UCB1, CheckerBoard, Kingsrow)**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --engines=puct,ucb1,checkerboard,kingsrow --games-per-pair=10 --time=0.5 --threads=8 --csv=results/tournament_all.csv
  ```

- **Fast Multi-Engine Benchmark across all detected CPU cores (`--threads=auto`)**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --engines=all --games-per-pair=20 --time=0.2 --threads=auto --csv=results/championship.csv
  ```

- **Custom 3-Way Tournament between Custom MCTS and Classic Checkerboard**:
  ```powershell
  .\build\Release\Damascus.exe --tournament --engines=puct,ucb1,checkerboard --games-per-pair=15 --time=1.0 --threads=4
  ```

---

##### 3. Benchmark Mode Examples (`--bench`)

- **Standard Search Throughput Benchmark (Default budgets: 0.2s, 1.0s, 3.0s)**:
  ```powershell
  .\build\Release\Damascus.exe --bench --csv=results/bench_default.csv
  ```

- **Custom Multi-Budget Scaling Scan on MCTS PUCT**:
  ```powershell
  .\build\Release\Damascus.exe --bench --budget=0.05,0.1,0.25,0.5,1.0,2.0,5.0 --csv=results/bench_scaling.csv
  ```

- **Single Rapid Benchmark Run**:
  ```powershell
  .\build\Release\Damascus.exe --bench --budget=0.5
  ```

---

##### 4. Genetic Algorithm Tuning Examples (`--tune` / `--tune-ga`)

- **Automated Hyperparameter Optimization for MCTS PUCT (16 population, 10 generations, 8 threads)**:
  ```powershell
  .\build\Release\Damascus.exe --tune --target=puct --pop=16 --generations=10 --time=0.2 --threads=8 --csv=results/ga_puct_tuned.csv
  ```

- **Hyperparameter Optimization for MCTS UCB1 (24 population, 5 generations, custom mutation)**:
  ```powershell
  .\build\Release\Damascus.exe --tune --target=ucb1 --pop=24 --generations=5 --mutation-rate=0.25 --mutation-scale=0.20 --threads=8 --csv=results/ga_ucb1_tuned.csv
  ```

- **Quick Parameter Tuning Run with Fixed Random Seed for Reproducibility**:
  ```powershell
  .\build\Release\Damascus.exe --tune --target=puct --pop=8 --generations=3 --seed=42 --threads=4
  ```

---

## 📖 In-Depth Documentation & Technical Reports

Explore our comprehensive technical reports and theoretical documentation in the [`doc/`](doc/) directory:

### 🏛️ [Code Architecture & Theoretical Foundations](doc/code_architecture.md)
> Dive deep into the algorithmic design and mathematical theory behind Damascus. This document provides an exhaustive breakdown of **Monte Carlo Tree Search (MCTS)** across its four fundamental phases (*Selection*, *Expansion*, *Simulation*, *Backpropagation*), complete formal mathematical formulations of **UCB1** and **PUCT**, parameter definitions, and a detailed theoretical comparison between uniform and prior-guided tree search policies.

### 📊 [Experimental Results & Benchmarks](doc/experiments_results.md)
> Comprehensive empirical evaluations, tournament standings, multi-engine rating benchmarks, time-scaling graphs, and automated Genetic Algorithm (GA) hyperparameter optimization results.

---

## 👥 Authors & Academic Context

- **Matteo Richard Gaudino**
- **Claudio Velotti**

Developed as the final project for the **Intelligent Web** exam, Academic Year **2025/2026**.

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. Third-party components (Kingsrow by Ed Gilbert, CheckerBoard Dama by Martin Fierz, GLFW, cglm, Nuklear) belong to their respective authors.

