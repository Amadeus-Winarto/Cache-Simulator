#pragma once

#include "cstdint"
#include "filesystem"
#include "optional"
#include "vector"

// TODO: Extend this to 4 cores
constexpr auto NUM_CORES = 2;

enum InstructionType { READ = 0, WRITE = 1, OTHER = 2, MEMORY = 3 };
using Value = uint32_t;

struct Instruction {
  InstructionType label;
  Value value;
};

auto parse_traces(std::string path_str)
    -> std::array<std::vector<Instruction>, NUM_CORES>;

auto to_string(const InstructionType &instr_type) -> std::string;

auto to_string(const Instruction &instr) -> std::string;