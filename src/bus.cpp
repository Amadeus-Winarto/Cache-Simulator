#include "bus.hpp"

Bus::Bus(int num_processors)
    : response_valid_bits(num_processors, false),
      response_is_present_bits(num_processors, false),
      response_wait_bits(num_processors, false){};

auto Bus::acquire(int controller_id) -> int {
  // Register if not already registered
  bool is_registered{false};
  for (const auto &id : registration_queue) {
    if (id == controller_id) {
      is_registered = true;
      break;
    }
  }
  if (!is_registered) {
    registration_queue.push_back(controller_id);
  }

  if (!owner_id) {
    owner_id = registration_queue.front();
    registration_queue.pop_front();
  }

  return owner_id == controller_id;
}

void Bus::release(int controller_id) {
  if (owner_id && owner_id.value() == controller_id) {
    owner_id = std::nullopt;
  }
}

auto Bus::get_owner_id() -> std::optional<int> { return owner_id; }