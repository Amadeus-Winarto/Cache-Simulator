#!/usr/bin/env python3

import subprocess
import multiprocessing
import os
from tqdm import tqdm

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


def run_testcase(protocol_testcase_pair):
    protocol, testcase = protocol_testcase_pair
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


num_cores = multiprocessing.cpu_count() // 2
print(f"Running on {num_cores} cores")
with multiprocessing.Pool(processes=num_cores) as pool:
    r = list(
        tqdm(
            pool.imap(
                run_testcase,
                [
                    (protocol, testcase)
                    for protocol in PROTOCOLS
                    for testcase in TESTCASES
                ],
            ),
            total=len(PROTOCOLS) * len(TESTCASES),
        )
    )
