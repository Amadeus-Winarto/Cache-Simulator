#pragma once

#include "cache.hpp"
#include "trace.hpp"
#include <tuple>
#include <vector>

using AddressCyclePair = std::tuple<ParsedAddress, int>;

class MESIWriteBuffer {
private:
  const int capacity;
  const int MEMORY_MISS_PENALTY;
  std::vector<AddressCyclePair> queue;

public:
  MESIWriteBuffer(int memory_miss_penalty, int capacity = -1)
      : capacity(capacity), MEMORY_MISS_PENALTY(memory_miss_penalty) {}

  auto add_to_queue(ParsedAddress parsed_address) -> bool;

  auto run_once() -> void;

  auto remove_if_present(ParsedAddress parsed_address) -> bool;
};