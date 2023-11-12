#pragma once
#include "bus.hpp"
#include "cache.hpp"
#include "statistics.hpp"
#include "trace.hpp"
#include "write_buffer.hpp"

#include <memory>

constexpr auto MEMORY_MISS_PENALTY = 100;

class MemoryController {
private:
  MESIWriteBuffer write_buffer;
  std::optional<int> pending_write_back;

  std::optional<int> pending_data_read;

  int delay = 0;

  std::shared_ptr<StatisticsAccumulator> stats_accum;

private:
  auto write_back_with_write_buffer(ParsedAddress parsed_address) -> bool;
  auto read_data_with_write_buffer(ParsedAddress parsed_address) -> bool;

  auto simple_write_back(ParsedAddress parsed_address) -> bool;
  auto simple_read_data(ParsedAddress parsed_address) -> bool;

public:
  MemoryController(std::shared_ptr<StatisticsAccumulator> stats_accum)
      : write_buffer(MEMORY_MISS_PENALTY), stats_accum(stats_accum){};

  auto is_done() -> bool;
  
  auto run_once() -> void;

  auto write_back(ParsedAddress parsed_address) -> bool;

  auto read_data(ParsedAddress parsed_address) -> bool;

  auto set_delay(int delay) -> void;
};