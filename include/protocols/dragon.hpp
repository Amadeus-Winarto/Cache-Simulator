#pragma once

#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "memory_controller.hpp"
#include "trace.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

enum class DragonStatus {
  E = 3,
  Sm = 2,
  Sc = 1,
  M = 4,
  I = 0 
};
auto to_string(const DragonStatus &status) -> std::string;

class DragonProtocol {
public:
  using Status = DragonStatus;

  static auto handle_read_miss(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller) -> Instruction;

  static auto handle_write_miss(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller) -> Instruction;

  static auto handle_read_hit(
      int controller_id, int32_t, ParsedAddress,
      std::vector<std::shared_ptr<CacheController<DragonProtocol>>> &,
      std::shared_ptr<Bus>, std::shared_ptr<CacheLine<Status>>,
      std::shared_ptr<MemoryController>) -> Instruction;

  static auto handle_write_hit(
      int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
      std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
          &cache_controllers,
      std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
      std::shared_ptr<MemoryController> memory_controller) -> Instruction;

  static auto state_transition(const BusRequest &request,
                               std::shared_ptr<CacheLine<DragonStatus>> line)
      -> void;
};