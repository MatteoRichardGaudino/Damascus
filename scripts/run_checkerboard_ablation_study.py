#!/usr/bin/env python3
import subprocess
import os
import sys
import time

DAMASCUS_EXE = os.path.abspath("build/Release/Damascus.exe")
RESULTS_DIR = os.path.abspath("doc/results")

configs = [
    # MCTS PUCT vs CheckerBoard
    {
        "name": "puct_book_on_db_on",
        "desc": "MCTS PUCT vs CheckerBoard (Book: ON, DB: ON)",
        "engine": "puct",
        "book_flag": "--book",
        "db_flag": "--db",
        "csv": "cb_study_puct_book_on_db_on.csv"
    },
    {
        "name": "puct_book_on_db_off",
        "desc": "MCTS PUCT vs CheckerBoard (Book: ON, DB: OFF)",
        "engine": "puct",
        "book_flag": "--book",
        "db_flag": "--no-db",
        "csv": "cb_study_puct_book_on_db_off.csv"
    },
    {
        "name": "puct_book_off_db_on",
        "desc": "MCTS PUCT vs CheckerBoard (Book: OFF, DB: ON)",
        "engine": "puct",
        "book_flag": "--no-book",
        "db_flag": "--db",
        "csv": "cb_study_puct_book_off_db_on.csv"
    },
    {
        "name": "puct_book_off_db_off",
        "desc": "MCTS PUCT vs CheckerBoard (Book: OFF, DB: OFF)",
        "engine": "puct",
        "book_flag": "--no-book",
        "db_flag": "--no-db",
        "csv": "cb_study_puct_book_off_db_off.csv"
    },
    # MCTS UCB1 vs CheckerBoard
    {
        "name": "ucb1_book_on_db_on",
        "desc": "MCTS UCB1 vs CheckerBoard (Book: ON, DB: ON)",
        "engine": "ucb1",
        "book_flag": "--book",
        "db_flag": "--db",
        "csv": "cb_study_ucb1_book_on_db_on.csv"
    },
    {
        "name": "ucb1_book_on_db_off",
        "desc": "MCTS UCB1 vs CheckerBoard (Book: ON, DB: OFF)",
        "engine": "ucb1",
        "book_flag": "--book",
        "db_flag": "--no-db",
        "csv": "cb_study_ucb1_book_on_db_off.csv"
    },
    {
        "name": "ucb1_book_off_db_on",
        "desc": "MCTS UCB1 vs CheckerBoard (Book: OFF, DB: ON)",
        "engine": "ucb1",
        "book_flag": "--no-book",
        "db_flag": "--db",
        "csv": "cb_study_ucb1_book_off_db_on.csv"
    },
    {
        "name": "ucb1_book_off_db_off",
        "desc": "MCTS UCB1 vs CheckerBoard (Book: OFF, DB: OFF)",
        "engine": "ucb1",
        "book_flag": "--no-book",
        "db_flag": "--no-db",
        "csv": "cb_study_ucb1_book_off_db_off.csv"
    },
]

def main():
    os.makedirs(RESULTS_DIR, exist_ok=True)
    total_start = time.time()
    print("================================================================================")
    print("  Damascus 3D - Dedicated Ablation Campaign: MCTS vs CheckerBoard")
    print("  8 Configurations x 50 Games = 400 Total Matches (Time: 2.0s/move, 12 Threads)")
    print("================================================================================")
    print(f"Executable: {DAMASCUS_EXE}")
    print(f"Results Directory: {RESULTS_DIR}")
    sys.stdout.flush()

    for idx, c in enumerate(configs, 1):
        csv_path = os.path.join(RESULTS_DIR, c["csv"])
        print(f"\n[{idx}/8] Running {c['desc']}...")
        cmd = [
            DAMASCUS_EXE,
            "--match",
            f"--white={c['engine']}",
            "--black=checkerboard",
            "--games=50",
            "--time=2.0",
            "--threads=12",
            c["book_flag"],
            c["db_flag"],
            f"--csv={csv_path}"
        ]
        print(f"Command: {' '.join(cmd)}")
        sys.stdout.flush()
        t0 = time.time()
        ret = subprocess.run(cmd)
        elapsed = time.time() - t0
        if ret.returncode != 0:
            print(f"[ERROR] Run failed with return code {ret.returncode}")
            sys.exit(1)
        print(f"[OK] Completed in {elapsed:.2f}s -> Saved {csv_path}")
        sys.stdout.flush()

    total_time = time.time() - total_start
    print(f"\n================================================================================")
    print(f"  All 8 Configurations (400 Matches) Completed Successfully in {total_time/60.0:.2f} min!")
    print(f"================================================================================")

if __name__ == '__main__':
    main()
