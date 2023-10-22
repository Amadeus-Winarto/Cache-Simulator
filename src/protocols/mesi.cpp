#include "protocols/mesi.hpp"

#include "bus.hpp"
#include "cache.hpp"
#include "memory_controller.hpp"
#include "trace.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>

auto acquire_bus(int controller_id, std::shared_ptr<Bus> bus) -> bool {
  // Take ownership of bus, if possible
  if (bus->owner_id && bus->owner_id != controller_id) {
    // Somebody "owns" the bus -> cannot process instruction now
    return false;
  }

  // Set ourselves as the owner of the bus so that nobody else can use it
  bus->owner_id = controller_id;
  return true;
}

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
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!acquire_bus(controller_id, bus)) {
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
      bus->response_valid_bits.at(i) = false;
      break;
    }
  }

  if (is_waiting) {
#ifdef DEBUG_FLAG
    std::cout << "\t<<< Waiting for Cache." << std::endl;
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
  std::for_each(bus->response_valid_bits.begin(),
                bus->response_valid_bits.end(),
                [](auto &&valid_bit) { valid_bit = false; });

  // Update cache line
  line->tag = parsed_address.tag;
  line->last_used = curr_cycle;

  if (!is_shared) {
    // Miss: Go to memory controller
    if (memory_controller->receive_bus_request(request)) {
      // Memory-to-cache transfer completed
      bus->owner_id = std::nullopt;
      line->status = Status::E;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
      return Instruction{InstructionType::OTHER, 0, std::nullopt};
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Memory." << std::endl;
#endif

      return instruction;
    }
  } else {
    // Cache-to-cache transfer completed
    line->status = Status::S;
    bus->owner_id = std::nullopt;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif

    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto MESIProtocol::handle_write_miss(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};
  if (!acquire_bus(controller_id, bus)) {
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
      bus->response_valid_bits.at(i) = false;
      break;
    }
  }

  if (is_waiting) {
#ifdef DEBUG_FLAG
    std::cout << "\t<<< Waiting for Cache." << std::endl;
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
  std::for_each(bus->response_valid_bits.begin(),
                bus->response_valid_bits.end(),
                [](auto &&valid_bit) { valid_bit = false; });

  // Update cache line
  line->tag = parsed_address.tag;
  line->last_used = curr_cycle;

  if (!is_shared) {
    // Miss: Go to memory controller
    if (memory_controller->receive_bus_request(request)) {
      // Memory-to-cache transfer completed
      line->status = MESIStatus::M;
      bus->owner_id = std::nullopt;
#ifdef DEBUG_FLAG
      std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
      return Instruction{InstructionType::OTHER, 0, std::nullopt};
    } else {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Memory." << std::endl;
#endif
      return instruction;
    }
  } else {
    // Cache-to-cache transfer completed
    line->status = Status::M;
    bus->owner_id = std::nullopt;
#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
}

auto MESIProtocol::handle_read_hit(
    int controller_id, int32_t, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>> &,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>>,
    std::shared_ptr<MemoryController>) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::READ, std::nullopt, parsed_address.address};
  if (!acquire_bus(controller_id, bus)) {
    return instruction;
  }

  // No bus transaction generated -> return immediately
  return Instruction{InstructionType::OTHER, 0, std::nullopt};
}

auto MESIProtocol::handle_write_hit(
    int controller_id, int32_t curr_cycle, ParsedAddress parsed_address,
    std::vector<std::shared_ptr<CacheController<MESIProtocol>>>
        &cache_controllers,
    std::shared_ptr<Bus> bus, std::shared_ptr<CacheLine<Status>> line,
    std::shared_ptr<MemoryController> memory_controller) -> Instruction {
  const auto instruction =
      Instruction{InstructionType::WRITE, std::nullopt, parsed_address.address};
  if (!acquire_bus(controller_id, bus)) {
    return instruction;
  }

  switch (line->status) {
  case MESIStatus::M: {
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::E: {
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::S: {
#ifdef DEBUG_FLAG
    std::stringstream ss;
    ss << "Cycle: " << curr_cycle << "\n"
       << "Processor " << controller_id << " requests SHARED WRITE at address "
       << parsed_address.address << "\n\tLine: " << to_string(line)
       << "\n\t>>> " << to_string(line) << std::endl;
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
        bus->response_valid_bits.at(i) = false;
        break;
      }
    }

    if (is_waiting) {
#ifdef DEBUG_FLAG
      std::cout << "\t<<< Waiting for Cache." << std::endl;
#endif
      // We are waiting for another cache to respond -> cannot process
      // instruction -> return the same instruction
      return instruction;
    }

    // Read response
    auto is_shared = std::reduce(
        bus->response_is_present_bits.begin(),
        bus->response_is_present_bits.end(), false,
        [](bool acc, bool is_present) { return acc || is_present; });

    // Invalidate all responses
    std::for_each(bus->response_valid_bits.begin(),
                  bus->response_valid_bits.end(),
                  [](auto &&valid_bit) { valid_bit = false; });

    // Update cache line
    line->tag = parsed_address.tag;
    line->last_used = curr_cycle;
    line->status = MESIStatus::M;
    bus->owner_id = std::nullopt;

#ifdef DEBUG_FLAG
    std::cout << "\t<<< " << to_string(line) << std::endl;
#endif
    return Instruction{InstructionType::OTHER, 0, std::nullopt};
  }
  case MESIStatus::I: {
    // Impossible!
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
  };
}
