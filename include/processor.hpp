#pragma once
#include "bus.hpp"
#include "cache_controller.hpp"
#include "trace.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>

template <typename Protocol> class Processor {
  int processor_id = 0;
  int curr_idx = -1;
  std::optional<Instruction> curr_instr;

  std::vector<Instruction> instruction_queue;
  std::shared_ptr<CacheController<Protocol>> cache_controller;

public:
  Processor(int processor_id, const std::vector<Instruction> &instruction_queue,
            std::shared_ptr<CacheController<Protocol>> cache_controller)
      : processor_id(processor_id), instruction_queue(instruction_queue),
        cache_controller(cache_controller){};

  auto progress() -> float {
    return curr_idx / static_cast<float>(instruction_queue.size()) * 100.0f;
  }

  auto get_processor_id() -> int { return processor_id; }

  auto is_done() -> bool {
    return curr_idx >= static_cast<int>(instruction_queue.size() - 1) &&
           !curr_instr;
  }

  auto get_interesting_cache_lines() {
    cache_controller->get_interesting_cache_lines();
  }

  auto run_once(int32_t curr_cycle) -> std::optional<Instruction> {
    if (is_done()) {
      return std::nullopt;
    }

    // Fetch instruction
    if (!curr_instr) {
      curr_idx += 1;
      curr_instr = instruction_queue.at(curr_idx);
    }

    auto [label, cycles_left, address] = curr_instr.value();

    switch (label) {
    case InstructionType::OTHER: {
      if (cycles_left > 1) {
        curr_instr->num_cycles = curr_instr->num_cycles.value() - 1;
      } else {
        // Instruction is completed -> retire instruction
        curr_instr = std::nullopt;
      }
      return curr_instr;
    }
    default: {
      auto instr = cache_controller->processor_request(label, address.value(),
                                                       curr_cycle);
      if (instr.label == InstructionType::OTHER &&
          instr.num_cycles.value() == 0) {
        curr_instr = std::nullopt;
      } else {
        curr_instr = instr;
      }
      return curr_instr;
    }
    }
  }
};
