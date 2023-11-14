#pragma once

#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "memory_controller.hpp"
#include "statistics.hpp"
#include "trace.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

template <typename ProtocolStatus> class Protocol {
private:
  static auto state_transition(const BusRequest &request,
                               std::shared_ptr<CacheLine<ProtocolStatus>> line)
      -> void;

public:
  using Status = ProtocolStatus;
  static auto handle_read_miss(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<Protocol<Status>>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller,
      std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction;

  static auto handle_write_miss(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<Protocol<Status>>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller,
      std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction;

  static auto handle_read_hit(
      int controller_id, int32_t, ParsedAddress,
      std::vector<std::shared_ptr<CacheController<Protocol<Status>>>> &,
      std::shared_ptr<Bus>, std::shared_ptr<CacheLine<Status>>,
      std::shared_ptr<MemoryController>,
      std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction;

  static auto handle_write_hit(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<Protocol<Status>>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller,
      std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction;

  static auto handle_bus_request(
      const BusRequest &request, std::shared_ptr<Bus> bus,
      int32_t controller_id,
      std::shared_ptr<std::tuple<BusRequest, int32_t>> pending_bus_request,
      bool is_hit, int32_t num_words_per_line,
      std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller,
      std::shared_ptr<StatisticsAccumulator> stats_accum)
      -> std::shared_ptr<std::tuple<BusRequest, int32_t>>;
};