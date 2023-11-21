#!/usr/bin/env python3

import os
import random


def make_write(num_instructions):
    return ["1 0x0"] * num_instructions


def make_read(num_instructions):
    return ["0 0x0"] * num_instructions


def make_read_write(num_instructions):
    return [str(random.randint(0, 1)) + " 0x0" for _ in range(num_instructions)]


if __name__ == "__main__":
    TEST_NAME = "random_read_write"
    NUM_CORES = 4
    NUM_INSTRUCTIONS = int(1e6)

    OUTPUT_DIR = f"tests/{TEST_NAME}"
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    for i in range(NUM_CORES):
        FILE_PATH = f"{OUTPUT_DIR}/{TEST_NAME}_{i}.data"

        if TEST_NAME == "random_read_write":
            instructions = make_read_write(NUM_INSTRUCTIONS)
        elif TEST_NAME == "all_write":
            instructions = make_write(NUM_INSTRUCTIONS)
        elif TEST_NAME == "one_write_many_reads":
            if i == 0:
                instructions = make_write(NUM_INSTRUCTIONS)
            else:
                instructions = make_read(NUM_INSTRUCTIONS)
        else:
            raise NotImplementedError(f"Invalid test name: {TEST_NAME}")

        with open(FILE_PATH, "w", encoding="utf-8") as f:
            instruction_str = "\n".join(instructions)
            f.write(instruction_str)
