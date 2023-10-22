#pragma once

#include "cstdint"
#include "filesystem"
#include "optional"
#include "vector"

// TODO: Extend this to 4 cores
constexpr auto NUM_CORES = 2;

enum InstructionType { READ = 0, WRITE = 1, OTHER = 2 };
using Value = uint32_t;

class Instruction {
public:
  InstructionType label;
  std::optional<int> num_cycles;
  std::optional<int> address;
  Instruction(InstructionType label, std::optional<int> num_cycles,
              std::optional<int> address)
      : label(label), num_cycles(num_cycles), address(address) {}
};

auto parse_traces(std::string path_str)
    -> std::array<std::vector<Instruction>, NUM_CORES>;

auto to_string(const InstructionType &instr_type) -> std::string;

auto to_string(const Instruction &instr) -> std::string;