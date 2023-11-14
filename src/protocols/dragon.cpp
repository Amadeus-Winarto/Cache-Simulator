#include "protocols/dragon.hpp"

#include "bus.hpp"
#include "cache.hpp"
#include "memory_controller.hpp"
#include "trace.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>

auto to_string(const DragonStatus &status) -> std::string {
  switch (status) {
  case DragonStatus::E:
    return "E";
  case DragonStatus::Sm:
    return "Sm";
  case DragonStatus::Sc:
    return "Sc";
  case DragonStatus::M:
    return "M";
  case DragonStatus::I:
    return "I";
  default:
    return "Unknown";
  }
}

auto DragonProtocol::handle_read_miss(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests READ at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  if (((line->status == DragonStatus::M || line->status == DragonStatus::Sm) &&
       bus->already_flush == false)) {
    // Write-back to Memory
    if (memory_controller->write_back(parsed_address)) {
      // Write-back completed! Invalidate the line so that the next time it is
      // called, it goes back to read-miss
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Finish writing LRU to memory" << std::endl;
#endif
      // Set already_flush to true so that the next time it is called, it does
      // not write-back again
      bus->already_flush = true;
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

  // Invalidate all responses lmao do i invalidate??
  std::for_each(bus->response_completed_bits.begin(),
                bus->response_completed_bits.end(),
                [](auto &&valid_bit) { valid_bit = false; });

  if (!is_shared) {
    // Miss: Go to memory controller
    if (memory_controller->read_data(parsed_address)) {
      // Memory-to-cache transfer completed ->  Update cache line
      line->tag = parsed_address.tag;
      line->last_used = curr_cycle;
      line->status = DragonStatus::E;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
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
    line->status = DragonStatus::Sm;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif

    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto DragonProtocol::handle_write_miss(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};

  if (!bus->acquire(controller_id)) {
    return instruction;
  }

#ifdef DEBUG_FLAG
  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests WRITE at address "
     << parsed_address.address << "\n\tLine: " << to_string(line) << "\n\t>>> "
     << to_string(line) << std::endl;
  std::cout << ss.str();
#endif

  if (((line->status == DragonStatus::M || line->status == DragonStatus::Sm) &&
       bus->already_flush == false)) {
    // Write-back to Memory
    const auto request = BusRequest{BusRequestType::Flush,
                                    parsed_address.address, controller_id};
    bus->request_queue = request;
    if (memory_controller->write_back(parsed_address)) {
      // Write-back completed!
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Finish writing LRU to memory" << std::endl;
#endif
      // Set already_flush to true so that the next time it is called, it does
      // not write-back again
      bus->already_flush = true;
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<<Writing LRU to memory" << std::endl;
#endif
      // Write-back is not done
      return instruction;
    }
  }

  // Send BusUpd
  auto request =
      BusRequest{BusRequestType::BusUpd, parsed_address.address, controller_id};

  if (line->status == DragonStatus::Sm) {
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
      // instruction
      // -> return the same instruction
      return instruction;
    }
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
      line->status = DragonStatus::M;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
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
    line->status = DragonStatus::Sm;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto DragonProtocol::handle_read_hit(
    int controller_id, int32_t, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<DragonProtocol>>> &,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>>,
    std::shared_ptr<MemoryController>) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!bus->acquire(controller_id)) {
    return instruction;
  }
  // No bus transaction generated -> return immediately
  bus->release(controller_id);
  return Instruction{InstructionType::OTHER, 0, std::nullopt};
}

auto DragonProtocol::handle_write_hit(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<DragonProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};
  // try and acquire bus
  if (!bus->acquire(controller_id)) {
    return instruction;
  }

  switch (line->status) {
  case DragonStatus::M: {
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case DragonStatus::E: {
    line->status = DragonStatus::M;
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case DragonStatus::I: {
    // Impossible!
    return instruction;
  }
  default: {
#ifdef DEBUG_FLAG
    std::stringstream ss;
    ss << "Cycle: " << curr_cycle << "\n"
       << "Processor " << controller_id << " requests SHARED WRITE at address "
       << parsed_address.address << "\n\tLine: " << to_string(line)
       << "\n\t>>> " << to_string(line) << std::endl;
    std::cout << ss.str();
#endif

    // Send BusUpd request
    auto request = BusRequest{BusRequestType::BusUpd, parsed_address.address,
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

    // clear bus
    std::for_each(bus->response_completed_bits.begin(),
                  bus->response_completed_bits.end(),
                  [](auto &&valid_bit) { valid_bit = false; });

    // Update cache line
    line->tag = parsed_address.tag;
    line->last_used = curr_cycle;
    line->status = DragonStatus::Sm;

#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    bus->release(controller_id);
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  }
}
auto DragonProtocol::state_transition(
    const BusRequest &request, std::shared_ptr<CacheLine<DragonStatus>> line)
    -> void {
  switch (request.type) {
  case BusRequestType::BusRd: {
    // Read request
    switch (line->status) {
    case Status::Sm: {
      line->status = Status::Sm;
    } break;
    case Status::M: {
      line->status = Status::Sm;
    } break;
    case Status::E: {
      line->status = Status::Sc;
    } break;
    case Status::Sc: {
      line->status = Status::Sc;
    } break;
    default: {
      break;
    }
    }
    break;
  }
  case BusRequestType::BusUpd: {
    // bus updates
    switch (line->status) {
    case Status::I: {
      line->status = Status::I; // do nth
    } break;
    default: {
      line->status = Status::Sc;
      break;
    }
    }
  } break;
  case BusRequestType::Flush: {
    std::cout << "FLUSH should not appear here!" << std::endl;
    std::exit(0);
  }
  case BusRequestType::BusRdX:
    std::cout << "BusRdX should not appear here!" << std::endl;
    std::exit(0);
    break;
  };
}

auto DragonProtocol::handle_bus_request(
    const BusRequest &request, std::shared_ptr<Bus> bus, int32_t controller_id,
    std::shared_ptr<std::tuple<BusRequest, int32_t>> pending_bus_request,
    bool is_hit, int32_t num_words_per_line,
    std::shared_ptr<CacheLine<DragonStatus>> line)
    -> std::shared_ptr<std::tuple<BusRequest, int32_t>> {
  // Respond to request
  if (!pending_bus_request) {
#ifdef DEBUG_FLAG
    std::cout << "Cache " << controller_id << " is not busy -> Serve request"
              << std::endl;
#endif

    bus->response_is_present_bits.at(controller_id) = is_hit;
    bus->response_wait_bits.at(controller_id) = is_hit;

    if (is_hit && request.type == BusRequestType::BusRd) {
#ifdef DEBUG_FLAG
      std::cout << "\tCache " << controller_id
                << " is hit! Initiate cache-to-cache transfer" << std::endl;
#endif
      // wait 2N cycles
      return std::make_shared<std::tuple<BusRequest, int32_t>>(
          std::make_tuple(request, 2 * num_words_per_line - 1));
    } else if (request.type == BusRequestType::BusUpd) {
#ifdef DEBUG_FLAG
      std::cout << "\tCache " << controller_id << " BusUpd send only word"
                << std::endl;
#endif
      // wait 2 cycles
      return std::make_shared<std::tuple<BusRequest, int32_t>>(
          std::make_tuple(request, 2 - 1));
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
      DragonProtocol::state_transition(request, line);
      return nullptr;
    }
  }
}