#pragma once
#include "bus.hpp"
#include "cache.hpp"
#include "trace.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <sys/types.h>
#include <thread>
#include <tuple>
#include <vector>

static constexpr auto CACHE_FLUSH_MULTIPLIER = 2;

template <typename Protocol> struct CacheController {
public:
  using Status = typename Protocol::Status;
  int controller_id = 0;
  std::optional<std::tuple<BusRequest, int>> pending_bus_request;
  Cache<Protocol> cache;
  std::shared_ptr<Bus> bus;

  std::vector<std::shared_ptr<CacheController<Protocol>>> cache_controllers;

public:
  CacheController(int id, int cache_size, int associativity, int block_size,
                  std::shared_ptr<Bus> bus)
      : controller_id(id), cache(cache_size, associativity, block_size),
        bus(bus) {}

  void register_cache_controllers(
      std::vector<std::shared_ptr<CacheController<Protocol>>>
          &cache_controllers) {
    this->cache_controllers = cache_controllers;
  }

  /**
   * @brief Process a processor request. Returns the resulting instruction
   *
   * @param instr_type
   * @param address
   * @param curr_cycle
   * @return Instruction
   */
  auto processor_request(InstructionType instr_type, uint32_t address,
                         int32_t curr_cycle) -> Instruction {

    switch (instr_type) {
    case InstructionType::OTHER: {
      // Invalid processor request!
      return Instruction{InstructionType::OTHER, 0};
    } break;
    case InstructionType::MEMORY: {
      auto parsed = parse_address(address);
      auto [line, is_hit] = is_address_present(parsed.set_index, parsed.tag);

      // Update cache state
      line->line.status = Status::E;
      return Instruction{InstructionType::OTHER, 0};
    }
    default: {
      auto parsed = parse_address(address);
      auto [line, is_hit] = is_address_present(parsed.set_index, parsed.tag);

      if (is_hit) {
        switch (instr_type) {
        case InstructionType::READ: {
          return Protocol::handle_read_hit(bus, parsed, line, curr_cycle);
        }
        case InstructionType::WRITE: {
          return Protocol::handle_write_hit(bus, parsed, line, curr_cycle);
        }
        default:
          return Instruction{InstructionType::OTHER, 0};
        }
      } else {
        switch (instr_type) {
        case InstructionType::READ: {
          return Protocol::handle_read_miss(controller_id, bus, parsed, line,
                                            cache_controllers, curr_cycle);
        }
        case InstructionType::WRITE: {
          return Protocol::handle_write_miss(controller_id, bus, parsed, line,
                                             cache_controllers, curr_cycle);
        }
        default:
          return Instruction{InstructionType::OTHER, 0};
        }
      }
    }
    }
    return Instruction{InstructionType::OTHER, 0};
  }

  auto get_interesting_cache_lines() {
    std::cout << "Cache " << controller_id << ": " << std::endl;
    for (std::shared_ptr<CacheSet<Status>> cache_set_ptr : cache.sets) {
      for (auto cache_line_ptr : cache_set_ptr->lines) {
        if (cache_line_ptr->line.status != Status::I) {
          std::cout << "\t" << to_string(cache_line_ptr->line) << std::endl;
        }
      }
    }
  }

  void receive_bus_request() {
    if (!bus->request_ready) {
      return;
    }
    // Request is ready

    if (bus->response_valid_bits.at(controller_id)) {
      // Response is given already -> ignore
      return;
    }

    const auto &request = bus->request_queue.front();
    // Always ignore requests from the same cache
    if (request.controller_id == controller_id) {
      bus->response_valid_bits.at(controller_id) = true;
      bus->response_is_present_bits.at(controller_id) = false;
      bus->num_responses++;
      return;
    }

    // Respond to request
    auto parsed_address = parse_address(request.address);
    auto [line, is_hit] =
        is_address_present(parsed_address.set_index, parsed_address.tag);

    if (pending_bus_request) {
      // There is a pending request -> serve that
      // Invariant: the pending request is the same as the incoming request,
      // due to atomic bus

      auto [request, cycles_left] = pending_bus_request.value();
      bus->response_valid_bits.at(controller_id) = true;
      bus->response_is_present_bits.at(controller_id) = true;
      if (--cycles_left == 0) {
        pending_bus_request = std::nullopt;
        bus->response_wait_bits.at(controller_id) = false;
      } else {
        pending_bus_request = std::make_tuple(request, cycles_left);
        bus->response_wait_bits.at(controller_id) = true;
      }
      bus->num_responses++;
    } else {
      bus->response_is_present_bits.at(controller_id) = is_hit;
      bus->response_valid_bits.at(controller_id) = true;
      bus->response_wait_bits.at(controller_id) = false;
      bus->num_responses++;
    }

    // Downgrade status if necessary
    switch (request.type) {
    case BusRequestType::BusRd: {
      // Read request
      switch (line->line.status) {
      case Status::E: {
        line->line.status = Status::S;
      } break;
      case Status::M: {
        pending_bus_request = std::make_tuple(
            request, CACHE_FLUSH_MULTIPLIER * cache.num_words_per_set - 1);
        line->line.status = Status::S;
      } break;
      default: {
        break;
      }
      }
      break;
    }
    case BusRequestType::BusRdX: {
      // Invalidation request
      switch (line->line.status) {
      case Status::M: {
        pending_bus_request = std::make_tuple(
            request, CACHE_FLUSH_MULTIPLIER * cache.num_words_per_set - 1);
        line->line.status = Status::I;
      } break;
      default: {
        line->line.status = Status::I;
        break;
      }
      }
    } break;
    }

    return;
  }

private:
  auto parse_address(uint32_t address) -> ParsedAddress {
    auto offset = address & ((1 << cache.num_offset_bits) - 1);
    auto set_index = (address >> cache.num_offset_bits) &
                     ((1 << cache.num_set_index_bits) - 1);
    auto tag = address >> (cache.num_offset_bits + cache.num_set_index_bits);
    return ParsedAddress{tag, set_index, offset, address};
  }

  /**
   * @brief Propose a line to be evicted from the cache. Uses the LRU policy.
   *
   * @param address
   * @return uint32_t
   */
  auto propose_evict(std::shared_ptr<CacheSet<Status>> set)
      -> std::shared_ptr<ProtectedCacheLine<Status>> {
    auto line_idx = 0;
    auto oldest_line_idx = 0;
    auto oldest = 0;
    for (auto &line : set->lines) {
      if (line->line.status == Status::I) {
        // Evict this line
        return line;
      } else if (line->line.last_used <= oldest) {
        oldest = line->line.last_used;
        oldest_line_idx = line_idx;
      }
      line_idx += 1;
    }

    return set->lines.at(oldest_line_idx);
  }

  auto is_address_present(uint32_t set_index, uint32_t tag)
      -> std::tuple<std::shared_ptr<ProtectedCacheLine<Status>>, bool> {
    auto &set = cache.sets.at(set_index);
    for (auto &line : set->lines) {
      if (line->line.tag == tag && line->line.status != Status::I) {
        // Tag is in cache and is valid
        return {line, true};
      }
    }
    auto line = propose_evict(set);
    return {line, false};
  }
};