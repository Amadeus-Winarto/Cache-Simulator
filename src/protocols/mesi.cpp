#include "protocols/mesi.hpp"
#include "trace.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <numeric>
#include <optional>

constexpr auto READ_MISS_PENALTY = 100;

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
    int controller_id, std::shared_ptr<Bus> bus, ParsedAddress parsed_address,
    std::shared_ptr<ProtectedCacheLine<Status>> &line, int32_t curr_cycle)
    -> Instruction {
  std::lock_guard<std::mutex> line_lock{line->mutex};

  // Take ownership of bus, if possible
  std::lock_guard<std::mutex> bus_lock{bus->request_mutex};
  if (bus->owner_id && bus->owner_id != controller_id) {
    // Somebody "owns" the bus -> cannot process instruction now
    return Instruction{InstructionType::READ, parsed_address.address};
  }
  bus->owner_id = controller_id; // Set ourselves as the owner of the bus so
                                 // that nobody else can use it

  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests READ at address "
     << parsed_address.address << "\n\tLine: " << to_string(line->line)
     << "\n\t>>> " << to_string(line->line) << std::endl;
  std::cout << ss.str();

  // Send BusRd request
  auto request =
      BusRequest{BusRequestType::BusRd, parsed_address.address, controller_id};
  bus->request_queue.at(0) = request;
  bus->request_ready.store(true);

  // Wait for response
  while (bus->num_responses.load(std::memory_order_acquire) < NUM_CORES) {
    std::this_thread::yield();
  }

  // Invalidate request
  bus->request_ready.store(false, std::memory_order_release);

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
    // We are waiting for another cache to respond -> cannot process instruction
    // -> return the same instruction
    return Instruction{InstructionType::READ, parsed_address.address};
  }

  // Read response
  auto is_shared =
      std::reduce(bus->response_is_present_bits.begin(),
                  bus->response_is_present_bits.end(), false,
                  [](bool acc, bool is_present) { return acc || is_present; });

  // Invalidate all responses
  std::for_each(bus->response_valid_bits.begin(),
                bus->response_valid_bits.end(),
                [](int &valid_bit) { valid_bit = false; });
  bus->num_responses = 0;

  // Update cache line
  line->line.tag = parsed_address.tag;
  line->line.last_used = curr_cycle;
  std::cout << "\t<<< " << to_string(line->line) << std::endl;

  if (!is_shared) {
    // Memory read incurs 100 cycles cost
    return Instruction{InstructionType::MEMORY, READ_MISS_PENALTY - 1};
  } else {
    // Cache-to-cache transfer incurs 0 cost
    return Instruction{InstructionType::OTHER, 0};
  }
}

auto MESIProtocol::handle_write_miss(
    int controller_id, std::shared_ptr<Bus> bus, ParsedAddress parsed_address,
    std::shared_ptr<ProtectedCacheLine<Status>> &line, int32_t curr_cycle)
    -> Instruction {
  std::lock_guard<std::mutex> lock{line->mutex};

  // Take ownership of bus, if possible
  std::lock_guard<std::mutex> bus_lock{bus->request_mutex};
  if (bus->owner_id && bus->owner_id != controller_id) {
    // Somebody "owns" the bus -> cannot process instruction now
    return Instruction{InstructionType::READ, parsed_address.address};
  }
  bus->owner_id = controller_id; // Set ourselves as the owner of the bus so
                                 // that nobody else can use it

  std::stringstream ss;
  ss << "Cycle: " << curr_cycle << "\n"
     << "Processor " << controller_id << " requests WRITE at address "
     << parsed_address.address << "\n\tLine: " << to_string(line->line)
     << "\n\t>>> " << to_string(line->line) << std::endl;
  std::cout << ss.str();

  // Send BusRdX request
  auto request =
      BusRequest{BusRequestType::BusRdX, parsed_address.address, controller_id};
  bus->request_queue.at(0) = request;
  bus->request_ready.store(true);

  // Wait for response
  while (bus->num_responses.load(std::memory_order_acquire) < NUM_CORES) {
    std::this_thread::yield();
  }

  // Invalidate request
  bus->request_ready.store(false, std::memory_order_release);

  // TODO: dummy bus request and response
  line->line.tag = parsed_address.tag;
  line->line.status = Status::M;
  line->line.last_used = curr_cycle;

  return Instruction{InstructionType::OTHER, READ_MISS_PENALTY - 1};
}

auto MESIProtocol::handle_read_hit(
    std::shared_ptr<Bus> bus, ParsedAddress address,
    std::shared_ptr<ProtectedCacheLine<Status>> &line, int32_t curr_cycle)
    -> Instruction {

  //   auto request = BusRequest{BusRequestType::BusRdX, address};
  //   bus->submit_request(request);
  return Instruction{InstructionType::OTHER, 0};
}

auto MESIProtocol::handle_write_hit(
    std::shared_ptr<Bus> bus, ParsedAddress address,
    std::shared_ptr<ProtectedCacheLine<Status>> &line, int32_t curr_cycle)
    -> Instruction {
  return Instruction{InstructionType::OTHER, 0};
}