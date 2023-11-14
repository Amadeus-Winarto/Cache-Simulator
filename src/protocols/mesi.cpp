#include "protocols/mesi.hpp"

#include "bus.hpp"
#include "cache.hpp"
#include "memory_controller.hpp"
#include "statistics.hpp"
#include "trace.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>

auto to_string(const MESIStatus &status) -> std::string {
  switch (status) {
  case MESIStatus::M:
    return "M";
  case MESIStatus::E:
    return "E";
  case MESIStatus::S:
    return "S";
  case MESIStatus::I:
    return "I";
  default:
    return "Unknown";
  }
}

// TODO: Complete MESI Protocol
auto MESIProtocol::handle_read_miss(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller,
    std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests READ MISS at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  if (line->status == MESIStatus::M && bus->already_flush == false) {
    // Initiate write-back to Memory
    if (memory_controller->write_back(parsed_address)) {
      // Write-back completed!
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Finish writing LRU to memory" << std::endl;
#endif

      // Set already_flush to true so that the next time it is called, it does
      // not write-back again
      bus->already_flush = true;
      stats_accum->on_bus_traffic(
          cache_controllers.at(controller_id)->cache.num_words_per_line);
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Writing LRU to memory" << std::endl;
#endif
      // Write-back is not done
      return instruction;
    }
  }

  // Send BusRd request
  const auto request =
      BusRequest{BusRequestType::BusRd, parsed_address.address, controller_id};
  bus->request_queue = request;

  // Get responses from other caches
  for (auto cache_controller : cache_controllers) {
    cache_controller->receive_bus_request();
  }

  // Check if any of the response is a PENDING response
  bool is_waiting = false;
  for (auto i = 0; i < NUM_CORES; i++) {
    if (bus->response_wait_bits.at(i) == true) {
      is_waiting = true;

      // Reset the pending cache's information
      bus->response_completed_bits.at(i) = false;
      break;
    }
  }

  if (is_waiting) {
#ifdef DEBUG_FLAG
    std::cout << "\t<<< Waiting for Cache..." << std::endl;
#endif
    // We are waiting for another cache to respond -> cannot process instruction
    // -> return the same instruction
    return instruction;
  }

  // Read response
  auto is_shared =
      std::reduce(bus->response_is_present_bits.begin(),
                  bus->response_is_present_bits.end(), false,
                  [](bool acc, bool is_present) { return acc || is_present; });

  // Invalidate all responses
  std::for_each(bus->response_completed_bits.begin(),
                bus->response_completed_bits.end(),
                [](auto &&valid_bit) { valid_bit = false; });

  if (!is_shared) {
    // Miss: Go to memory controller
    if (memory_controller->read_data(parsed_address)) {
      // Memory-to-cache transfer completed ->  Update cache line
      line->tag = parsed_address.tag;
      line->last_used = curr_cycle;
      line->status = Status::E;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
      stats_accum->on_bus_traffic(
          cache_controllers.at(controller_id)->cache.num_words_per_line);
      bus->release(controller_id);
      return Instruction{InstructionType::OTHER, 0, std::nullopt};
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Memory..." << std::endl;
#endif

      return instruction;
    }
  } else {
    // Cache-to-cache transfer completed -> Update cache line
    line->tag = parsed_address.tag;
    line->last_used = curr_cycle;
    line->status = Status::S;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif

    stats_accum->on_bus_traffic(
        cache_controllers.at(controller_id)->cache.num_words_per_line);
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto MESIProtocol::handle_write_miss(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller,
    std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests WRITE MISS at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  if (line->status == MESIStatus::M && bus->already_flush == false) {
    // Write-back to Memory
    if (memory_controller->write_back(parsed_address)) {
      // Write-back completed!
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Finish writing LRU to memory" << std::endl;
#endif

      // Set already_flush to true so that the next time it is called, it does
      // not write-back again
      stats_accum->on_bus_traffic(
          cache_controllers.at(controller_id)->cache.num_words_per_line);
      bus->already_flush = true;
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Writing LRU to memory" << std::endl;
#endif
      // Write-back is not done
      return instruction;
    }
  }

  // Send BusRdX request
  auto request =
      BusRequest{BusRequestType::BusRdX, parsed_address.address, controller_id};
  bus->request_queue = request;

  // Wait for response
  for (auto cache_controller : cache_controllers) {
    cache_controller->receive_bus_request();
  }

  // Check if any of the response is a PENDING response
  bool is_waiting = false;
  for (auto i = 0; i < NUM_CORES; i++) {
    if (bus->response_wait_bits.at(i) == true) {
      is_waiting = true;

      // Reset the pending cache's information
      bus->response_completed_bits.at(i) = false;
      break;
    }
  }

  if (is_waiting) {
#ifdef DEBUG_FLAG
    std::cout << "\t<<< Waiting for Cache..." << std::endl;
#endif
    // We are waiting for another cache to respond -> cannot process instruction
    // -> return the same instruction
    return instruction;
  }

  // Read response
  auto is_shared =
      std::reduce(bus->response_is_present_bits.begin(),
                  bus->response_is_present_bits.end(), false,
                  [](bool acc, bool is_present) { return acc || is_present; });

  // Invalidate all responses
  std::for_each(bus->response_completed_bits.begin(),
                bus->response_completed_bits.end(),
                [](auto &&valid_bit) { valid_bit = false; });

  if (!is_shared) {
    // Miss: Go to memory controller
    if (memory_controller->read_data(parsed_address)) {
      // Memory-to-cache transfer completed -> Update cache line
      line->tag = parsed_address.tag;
      line->last_used = curr_cycle;
      line->status = MESIStatus::M;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
      stats_accum->on_bus_traffic(
          cache_controllers.at(controller_id)->cache.num_words_per_line);
      bus->release(controller_id);
      return Instruction{InstructionType::OTHER, 0, std::nullopt};
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Memory..." << std::endl;
#endif
      return instruction;
    }
  } else {
    // Cache-to-cache transfer completed -> Update cache line
    line->tag = parsed_address.tag;
    line->last_used = curr_cycle;
    line->status = Status::M;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    stats_accum->on_bus_traffic(
        cache_controllers.at(controller_id)->cache.num_words_per_line);
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto MESIProtocol::handle_read_hit(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>> &,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController>,
    std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction {

  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests READ HIT at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  // Optimisation: allow read hits to be processed without acquiring the bus
  bus->release(controller_id);
  return Instruction{InstructionType::OTHER, 0, std::nullopt};
}

auto MESIProtocol::handle_write_hit(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller,
    std::shared_ptr<StatisticsAccumulator> stats_accum) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests WRITE HIT at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  switch (line->status) {
  case MESIStatus::M: {
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::E: {
    bus->release(controller_id);
    line->status = MESIStatus::M;
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::S: {
#ifdef DEBUG_FLAG
    std::stringstream ss;
    ss << "\tWrite from Shared..." << std::endl;
    std::cout << ss.str();
#endif

    // Send BusRdX request
    auto request = BusRequest{BusRequestType::BusRdX, parsed_address.address,
                              controller_id};
    bus->request_queue = request;

    // Wait for response
    for (auto cache_controller : cache_controllers) {
      cache_controller->receive_bus_request();
    }

    // Check if any of the response is a PENDING response
    bool is_waiting = false;
    for (auto i = 0; i < NUM_CORES; i++) {
      if (bus->response_wait_bits.at(i) == true) {
        is_waiting = true;

        // Reset the pending cache's information
        bus->response_completed_bits.at(i) = false;
        break;
      }
    }

    if (is_waiting) {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Cache..." << std::endl;
#endif
      // We are waiting for another cache to respond -> cannot process
      // instruction -> return the same instruction
      return instruction;
    }

    // Invalidate all responses
    std::for_each(bus->response_completed_bits.begin(),
                  bus->response_completed_bits.end(),
                  [](auto &&valid_bit) { valid_bit = false; });

    // Update cache line
    line->tag = parsed_address.tag;
    line->last_used = curr_cycle;
    line->status = MESIStatus::M;

#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::I: {
    // Impossible!
    std::cout << "Impossible!" << std::endl;
    return instruction;
  }
  }
}
auto MESIProtocol::state_transition(const BusRequest &request,
                                    std::shared_ptr<CacheLine<MESIStatus>> line)
    -> void {
  switch (request.type) {
  case BusRequestType::BusRd: {
    // Read request
    switch (line->status) {
    case Status::E: {
      line->status = Status::S;
    } break;
    case Status::M: {
      line->status = Status::S;
    } break;
    default: {
      break;
    }
    }
    break;
  }
  case BusRequestType::BusRdX: {
    // Invalidation request
    switch (line->status) {
    case Status::M: {
      line->status = Status::I;
    } break;
    default: {
      line->status = Status::I;
      break;
    }
    }
  } break;
  case BusRequestType::Flush: {
    std::cout << "FLUSH should not appear here!" << std::endl;
    std::exit(0);
  }
  case BusRequestType::BusUpd:
    std::cout << "BUSUPD should not appear here!" << std::endl;
    std::exit(0);
    break;
  };
}

auto MESIProtocol::handle_bus_request(
    const BusRequest &request, std::shared_ptr<Bus> bus, int32_t controller_id,
    std::shared_ptr<std::tuple<BusRequest, int32_t>> pending_bus_request,
    bool is_hit, int32_t num_words_per_line,
    std::shared_ptr<CacheLine<MESIStatus>> line,
    std::shared_ptr<StatisticsAccumulator> stats_accum)
    -> std::shared_ptr<std::tuple<BusRequest, int32_t>> {
  // Respond to request
  if (!pending_bus_request) {
#ifdef DEBUG_FLAG
    std::cout << "Cache " << controller_id << " is not busy -> Serve request"
              << std::endl;
#endif

    bus->response_is_present_bits.at(controller_id) = is_hit;
    bus->response_wait_bits.at(controller_id) = is_hit;

    if (is_hit) {
#ifdef DEBUG_FLAG
      std::cout << "\tCache " << controller_id
                << " is hit! Initiate cache-to-cache transfer" << std::endl;
#endif
      return std::make_shared<std::tuple<BusRequest, int32_t>>(
          std::make_tuple(request, 2 * num_words_per_line - 1));
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\tCache " << controller_id << " is miss!" << std::endl;
#endif
      bus->response_completed_bits.at(controller_id) = true;
      return nullptr;
    }
  } else {
#ifdef DEBUG_FLAG
    std::cout << "Cache " << controller_id << " sending cache line..."
              << std::endl;
#endif

    // There is a pending request -> serve that
    // Invariant: the pending request is the same as the incoming request,
    // due to atomic bus
    auto [request, cycles_left] = *pending_bus_request;

    bus->response_is_present_bits.at(controller_id) = true;
    if (cycles_left > 1) {
      bus->response_wait_bits.at(controller_id) = true;
      return std::make_shared<std::tuple<BusRequest, int32_t>>(
          std::make_tuple(request, cycles_left - 1));
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\tCache " << controller_id << " finished sending cache line"
                << std::endl;
#endif
      bus->response_completed_bits.at(controller_id) = true;
      bus->response_wait_bits.at(controller_id) = false;

      // Downgrade status if necessary
      if (request.type == BusRequestType::BusRdX) {
        stats_accum->on_invalidate(controller_id);
      }
      MESIProtocol::state_transition(request, line);
      return nullptr;
    }
  }
}