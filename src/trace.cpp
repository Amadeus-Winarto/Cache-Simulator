#include "trace.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

static constexpr auto read_instruction_type(int x)
    -> std::optional<InstructionType> {
  if (x == 0) {
    return InstructionType::READ;
  } else if (x == 1) {
    return InstructionType::WRITE;
  } else if (x == 2) {
    return InstructionType::OTHER;
  } else {
    return std::nullopt;
  }
}

auto read_trace(const std::filesystem::path &path) -> std::vector<Instruction> {
  auto filestream = std::ifstream{path.c_str(), std::ios::binary};
  auto instructions = std::vector<Instruction>{};

  std::string line;
  while (std::getline(filestream, line)) {
    std::istringstream iss(line);

    int label;
    std::string value_str;
    if (!(iss >> label >> value_str)) {
      break;
    }

    auto type = read_instruction_type(label);
    if (!type.has_value()) {
      std::cerr << "Instruction type: " << label << " is invalid!";
      std::exit(1);
    }
    auto instruction_type = type.value();
    Value value = std::stoul(value_str, nullptr, 16);

    switch (instruction_type) {
    case InstructionType::OTHER: {
      instructions.push_back(
          Instruction{instruction_type, value, std::nullopt});
    } break;
    default: {
      instructions.push_back(
          Instruction{instruction_type, std::nullopt, value});
    } break;
    }
  }

  return instructions;
}

auto parse_traces(std::string path_str)
    -> std::array<std::vector<Instruction>, NUM_CORES> {

  auto dirpath = std::filesystem::path{path_str};

  if (!std::filesystem::exists(dirpath)) {
    std::cerr << "Given path: " << dirpath << " does not exist!" << std::endl;
    std::exit(1);
  }

  if (!std::filesystem::is_directory(dirpath)) {
    std::cerr << "Given path: " << dirpath << " is not a directory!"
              << std::endl;
    std::exit(1);
  }

  const auto benchmark_name = dirpath.filename();
  std::cout << "Running benchmark: " << benchmark_name << std::endl;

  std::array<std::vector<Instruction>, NUM_CORES> instructions;
  for (int i = 0; i < NUM_CORES; i++) {
    std::stringstream ss;
    ss << benchmark_name.c_str() << "_" << i << ".data";
    auto filename = ss.str();
    auto filepath = dirpath;
    filepath.append(filename);

    if (!std::filesystem::exists(filepath)) {
      std::cerr << "Test file: " << filepath.c_str() << " does not exist!"
                << std::endl;
      std::exit(1);
    }
    instructions.at(i) = read_trace(filepath);
  }

  std::cout << "Trace parsed successfully!" << std::endl;
  return instructions;
}

auto to_string(const InstructionType &instr_type) -> std::string {
  switch (instr_type) {
  case InstructionType::READ:
    return "READ";
  case InstructionType::WRITE:
    return "WRITE";
  case InstructionType::OTHER:
    return "OTHER";
  case InstructionType::MEMORY:
    return "MEMORY";
  }
  return "INVALID";
}

auto to_string(const Instruction &instr) -> std::string {
  std::stringstream ss;
  ss << to_string(instr.label) << " at address " << instr.address.value_or(-1);
  return ss.str();
}