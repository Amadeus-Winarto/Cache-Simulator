#pragma once

#include "cache.hpp"
#include "trace.hpp"
#include <tuple>
#include <vector>

using AddressCyclePair = std::tuple<uint32_t, int>;

class MESIWriteBuffer {
private:
  const int capacity;
  const int MEMORY_MISS_PENALTY;
  std::vector<AddressCyclePair> queue;

public:
  MESIWriteBuffer(int memory_miss_penalty, int capacity = -1)
      : capacity(capacity), MEMORY_MISS_PENALTY(memory_miss_penalty) {}

  auto add_to_queue(uint32_t address) -> bool;

  auto run_once() -> bool;

  auto is_empty() -> bool;

  auto remove_if_present(uint32_t uint32_t) -> bool;
};