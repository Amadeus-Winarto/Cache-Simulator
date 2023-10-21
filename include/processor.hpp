#pragma once
#include "bus.hpp"
#include "cache_controller.hpp"
#include "trace.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>

template <typename Protocol> class Processor {
  int processor_id = 0;
  int curr_idx = -1;
  std::optional<Instruction> curr_instr;

  std::vector<Instruction> instruction_queue;
  CacheController<Protocol> cache_controller;

public:
  Processor(int processor_id, const std::vector<Instruction> &instruction_queue,
            int cache_size, int associativity, int block_size,
            std::shared_ptr<Bus> bus)
      : processor_id(processor_id), instruction_queue(instruction_queue),
        cache_controller(processor_id, cache_size, associativity, block_size,
                         bus){};

  auto get_processor_id() -> int { return processor_id; }

  auto is_done() -> bool {
    return curr_idx >= static_cast<int>(instruction_queue.size() - 1) &&
           !curr_instr;
  }

  auto start_cache_controller() -> void {
    cache_controller.stop_thread.store(false, std::memory_order::relaxed);
  }

  auto pause_cache_controller() -> void {
    cache_controller.stop_thread.store(true, std::memory_order::relaxed);
  }

  auto stop_cache_controller() -> void {
    pause_cache_controller();
    cache_controller.bus_thread.request_stop();
  }

  auto get_interesting_cache_lines() {
    cache_controller.get_interesting_cache_lines();
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

    auto [label, value] = curr_instr.value();

    switch (label) {
    case InstructionType::MEMORY: {
      const auto cycles_left = value;
      if (cycles_left > 1) {
        curr_instr->value -= 1;
      } else {
        // Memory read / write is completed -> retire instruction
        curr_instr = std::nullopt;
        cache_controller.processor_request(label, value, curr_cycle);

        // Release bus ownership
        std::lock_guard<std::mutex> lock{cache_controller.bus->request_mutex};
        cache_controller.bus->owner_id = std::nullopt;
      }
      return curr_instr;
    }
    case InstructionType::OTHER: {
      const auto cycles_left = value;
      if (cycles_left > 1) {
        curr_instr->value -= 1;
      } else {
        // Instruction is completed -> retire instruction
        curr_instr = std::nullopt;
      }
      return curr_instr;
    }
    default: {
      auto address = value;
      auto instr =
          cache_controller.processor_request(label, address, curr_cycle);
      if (instr.label == InstructionType::OTHER && instr.value == 0) {
        curr_instr = std::nullopt;
      } else {
        curr_instr = instr;
      }
      return curr_instr;
    }
    }
  }
};
