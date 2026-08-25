#!/usr/bin/env python3
"""
Damascus Phase 7 Master Experiment Orchestrator
Automates all experimental pipelines with checkpointing, real-time logging, and CSV exports.
"""

import os
import sys
import json
import time
import subprocess

DAMASCUS_EXE = os.path.abspath("build/Release/Damascus.exe")
RESULTS_DIR = os.path.abspath("doc/results")
MANIFEST_PATH = os.path.join(RESULTS_DIR, "manifest.json")

os.makedirs(RESULTS_DIR, exist_ok=True)

def load_manifest():
    if os.path.exists(MANIFEST_PATH):
        try:
            with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}

def save_manifest(manifest):
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

def run_cmd(cmd, step_name):
    print(f"\n{'='*80}")
    print(f"  [PHASE 7 STEP] {step_name}")
    print(f"  Command: {' '.join(cmd)}")
    print(f"{'='*80}\n")
    sys.stdout.flush()

    t0 = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    
    output_lines = []
    for line in iter(proc.stdout.readline, ''):
        print(line, end='')
        output_lines.append(line)
        sys.stdout.flush()
    proc.stdout.close()
    returncode = proc.wait()
    elapsed = time.time() - t0
    
    print(f"\n-> Step '{step_name}' finished in {elapsed:.2f}s with returncode {returncode}\n")
    sys.stdout.flush()
    return returncode, elapsed, "".join(output_lines)

def run_experiment(step_id, step_name, cmd, output_csv, manifest):
    if manifest.get(step_id, {}).get("status") == "completed" and os.path.exists(output_csv) and os.path.getsize(output_csv) > 0:
        print(f"[CHECKPOINT] Step '{step_id}' already completed ({output_csv}). Skipping.")
        return True

    print(f"[START] Executing Step '{step_id}': {step_name}")
    rc, elapsed, log_out = run_cmd(cmd, step_name)
    
    if rc == 0 and os.path.exists(output_csv) and os.path.getsize(output_csv) > 0:
        manifest[step_id] = {
            "status": "completed",
            "name": step_name,
            "csv": os.path.relpath(output_csv, os.getcwd()),
            "elapsed_seconds": elapsed,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        save_manifest(manifest)
        return True
    else:
        print(f"[ERROR] Step '{step_id}' failed with exit code {rc} or missing CSV.")
        manifest[step_id] = {
            "status": "failed",
            "returncode": rc,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        save_manifest(manifest)
        return False

def main():
    manifest = load_manifest()
    total_start = time.time()
    print("================================================================================")
    print("  Damascus 3D - Italian Draughts MCTS AI Engine")
    print("  Phase 7 Experimental Campaign & Hyperparameter Tuning Pipeline")
    print("================================================================================")
    print(f"Executable: {DAMASCUS_EXE}")
    print(f"Results Directory: {RESULTS_DIR}")
    print(f"Threads: 12")
    sys.stdout.flush()

    # Step 0.1: Hyperparameter Tuning GA for MCTS PUCT
    csv_tune_puct = os.path.join(RESULTS_DIR, "tune_ga_puct.csv")
    cmd_tune_puct = [
        DAMASCUS_EXE, "--tune", "--target=puct",
        "--pop=10", "--generations=4", "--games-per-pair=2",
        "--time=0.05", "--max-plies=80", "--threads=12", f"--csv={csv_tune_puct}", "--seed=42"
    ]
    if not run_experiment("tune_ga_puct", "GA Hyperparameter Tuning (MCTS PUCT)", cmd_tune_puct, csv_tune_puct, manifest):
        sys.exit(1)

    # Step 0.2: Hyperparameter Tuning GA for MCTS UCB1
    csv_tune_ucb1 = os.path.join(RESULTS_DIR, "tune_ga_ucb1.csv")
    cmd_tune_ucb1 = [
        DAMASCUS_EXE, "--tune", "--target=ucb1",
        "--pop=10", "--generations=4", "--games-per-pair=2",
        "--time=0.05", "--max-plies=80", "--threads=12", f"--csv={csv_tune_ucb1}", "--seed=42"
    ]
    if not run_experiment("tune_ga_ucb1", "GA Hyperparameter Tuning (MCTS UCB1)", cmd_tune_ucb1, csv_tune_ucb1, manifest):
        sys.exit(1)

    # Step 1: Time Scaling Analysis (0.2s vs 1.0s vs 3.0s)
    csv_time_scaling = os.path.join(RESULTS_DIR, "exp1_time_scaling.csv")
    cmd_time_scaling = [
        DAMASCUS_EXE, "--tournament", "--engines=puct,ucb1",
        "--time=0.2", "--games-per-pair=10", "--threads=12", f"--csv={csv_time_scaling}"
    ]
    if not run_experiment("exp1_time_scaling", "Experiment 1: Time Scaling & Inter-Engine Matching", cmd_time_scaling, csv_time_scaling, manifest):
        sys.exit(1)

    # Step 2.1: Policy Comparison Tournament (Fast Profile: 0.2s)
    csv_tour_fast = os.path.join(RESULTS_DIR, "exp2_tournament_fast.csv")
    cmd_tour_fast = [
        DAMASCUS_EXE, "--tournament", "--engines=puct,ucb1,checkerboard,kingsrow,random",
        "--time=0.2", "--games-per-pair=6", "--threads=12", f"--csv={csv_tour_fast}"
    ]
    if not run_experiment("exp2_tournament_fast", "Experiment 2.1: Head-to-Head Tournament (0.2s Fast)", cmd_tour_fast, csv_tour_fast, manifest):
        sys.exit(1)

    # Step 2.2: Policy Comparison Tournament (Medium Profile: 1.0s)
    csv_tour_med = os.path.join(RESULTS_DIR, "exp2_tournament_medium.csv")
    cmd_tour_med = [
        DAMASCUS_EXE, "--tournament", "--engines=puct,ucb1,checkerboard,kingsrow,random",
        "--time=1.0", "--games-per-pair=4", "--threads=12", f"--csv={csv_tour_med}"
    ]
    if not run_experiment("exp2_tournament_medium", "Experiment 2.2: Head-to-Head Tournament (1.0s Medium)", cmd_tour_med, csv_tour_med, manifest):
        sys.exit(1)

    # Step 2.3: Policy Comparison Tournament (Slow Profile: 3.0s)
    csv_tour_slow = os.path.join(RESULTS_DIR, "exp2_tournament_slow.csv")
    cmd_tour_slow = [
        DAMASCUS_EXE, "--tournament", "--engines=puct,ucb1,checkerboard,kingsrow,random",
        "--time=3.0", "--games-per-pair=2", "--threads=12", f"--csv={csv_tour_slow}"
    ]
    if not run_experiment("exp2_tournament_slow", "Experiment 2.3: Head-to-Head Tournament (3.0s Slow)", cmd_tour_slow, csv_tour_slow, manifest):
        sys.exit(1)

    # Step 3.1: Rollout Throughput Benchmark
    csv_throughput = os.path.join(RESULTS_DIR, "exp3_rollout_throughput.csv")
    cmd_throughput = [
        DAMASCUS_EXE, "--bench", "--budget=0.2,1.0,3.0", f"--csv={csv_throughput}"
    ]
    if not run_experiment("exp3_throughput", "Experiment 3.1: Search Throughput & MCTS Node Speed Benchmark", cmd_throughput, csv_throughput, manifest):
        sys.exit(1)

    # Step 3.2: Ablation Study Matches (Biased vs Random Rollouts, Tablebase On vs Off)
    csv_ablation = os.path.join(RESULTS_DIR, "exp3_ablation_matches.csv")
    cmd_ablation = [
        DAMASCUS_EXE, "--match", "--white=puct", "--black=puct",
        "--time=0.2", "--games=20", "--threads=12", f"--csv={csv_ablation}"
    ]
    if not run_experiment("exp3_ablation", "Experiment 3.2: Ablation Matches", cmd_ablation, csv_ablation, manifest):
        sys.exit(1)

    # Step 4.1: Opening Book Head-to-Head Tournament (50 games ODB vs No-Book)
    csv_book_tour = os.path.join(RESULTS_DIR, "exp4_opening_book_tournament.csv")
    cmd_book_tour = [
        DAMASCUS_EXE, "--test-opening-tournament", "--games=50", "--threads=12", f"--csv={csv_book_tour}"
    ]
    if not run_experiment("exp4_book_tour", "Experiment 4.1: Opening Book 50-Game Head-to-Head Tournament", cmd_book_tour, csv_book_tour, manifest):
        sys.exit(1)

    # Step 4.2: Endgame Tablebase Accuracy & Solving Benchmark (100 games)
    csv_endgames = os.path.join(RESULTS_DIR, "exp4_endgame_solver.csv")
    cmd_endgames = [
        DAMASCUS_EXE, "--test-endgames", "--wld-backend=official", f"--csv={csv_endgames}"
    ]
    if not run_experiment("exp4_endgame_solver", "Experiment 4.2: Endgame Tablebase 100-Position Solving Test", cmd_endgames, csv_endgames, manifest):
        sys.exit(1)

    total_time = time.time() - total_start
    print("================================================================================")
    print(f"  ALL PHASE 7 EXPERIMENTS COMPLETED SUCCESSFULLY IN {total_time/60.0:.2f} MINUTES!")
    print("================================================================================")

if __name__ == "__main__":
    main()
