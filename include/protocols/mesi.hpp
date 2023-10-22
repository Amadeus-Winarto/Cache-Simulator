#pragma once

#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "trace.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

enum class MESIStatus {
  M = 3,
  E = 2,
  S = 1,
  I = 0 // default
};
auto to_string(const MESIStatus &status) -> std::string;

class MESIProtocol {
public:
  using Status = MESIStatus;

  static auto
  handle_read_miss(int controller_id, std::shared_ptr<Bus> bus,
                   ParsedAddress parsed_address,
                   std::shared_ptr<ProtectedCacheLine<Status>> &line,
                   std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
                       &cache_controllers,
                   int32_t curr_cycle) -> Instruction;

  static auto
  handle_write_miss(int controller_id, std::shared_ptr<Bus> bus,
                    ParsedAddress address,
                    std::shared_ptr<ProtectedCacheLine<Status>> &line,
                    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
                        &cache_controllers,
                    int32_t curr_cycle) -> Instruction;

  /**
   * @brief Upon read hit, submit a BusRd request to the bus.
   *
   * @param bus
   * @param address
   * @return Instruction
   */
  static auto handle_read_hit(std::shared_ptr<Bus> bus, ParsedAddress address,
                              std::shared_ptr<ProtectedCacheLine<Status>> &line,
                              int32_t curr_cycle) -> Instruction;

  static auto
  handle_write_hit(std::shared_ptr<Bus> bus, ParsedAddress address,
                   std::shared_ptr<ProtectedCacheLine<Status>> &line,
                   int32_t curr_cycle) -> Instruction;
};