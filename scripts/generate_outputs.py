#!/usr/bin/env python3

import subprocess
import multiprocessing
import os

PROGRAM = "build/coherence"
TEST_DIR = "tests"
PROTOCOLS = ["MESI", "MOESI", "Dragon", "MESIF"]
TESTCASES = [
    "blackscholes",
    "bodytrack",
    "fluidanimate",
    "all_write",
    "one_write_many_reads",
    "random_read_write",
]
OUTPUT_DIR = "outputs/refactored"

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)


def run_testcase(protocol, testcase):
    print("*", end="", flush=True)
    with open(
        f"{OUTPUT_DIR}/{protocol}_{testcase}.out".format(TEST_DIR, testcase),
        "w",
        encoding="utf-8",
    ) as f:
        subprocess.run(
            f"{PROGRAM} {protocol} {TEST_DIR}/{testcase}",
            shell=True,
            stdout=f,
            check=True,
        )


with multiprocessing.Pool(processes=multiprocessing.cpu_count() // 2) as pool:
    pool.starmap(
        run_testcase,
        [(protocol, testcase) for protocol in PROTOCOLS for testcase in TESTCASES],
    )
