#include "bus.hpp"
#include <iostream>

Bus::Bus(int num_processors)
    : response_completed_bits(num_processors, false),
      response_is_present_bits(num_processors, false),
      response_wait_bits(num_processors, false){};

auto Bus::acquire(int controller_id) -> int {
  if (just_released) {
    return false;
  }

  if (!owner_id) {
    // Not owner -> make yourself owner
    owner_id = controller_id;
    return true;
  } else if (owner_id && owner_id.value() == controller_id) {
    // Got owner and I'm owner
    return true;
  } else {
    // Put yourself in queue, if you're not already in the queue
    auto is_in_queue{false};
    for (const auto x : registration_queue) {
      if (x == controller_id) {
        is_in_queue = true;
        break;
      }
    }

    if (!is_in_queue) {
      registration_queue.push_back(controller_id);
    }

    return false;
  }
}

void Bus::release(int controller_id) {
  // Release bus if owner
  if (owner_id && owner_id.value() == controller_id) {
    owner_id = std::nullopt;

    // Get next owner if any
    if (!registration_queue.empty()) {
      owner_id = registration_queue.front();
      registration_queue.pop_front();
    }
  } else if (!owner_id) {
    std::cout << "Error: Bus is not owned by anyone but Core " << controller_id
              << " wants to release. " << std::endl;
    std::exit(1);
  } else if (owner_id && owner_id.value() != controller_id) {
    std::cout << "Error: Bus is owned by " << owner_id.value() << " but core "
              << controller_id << " wants to release. " << std::endl;
    std::exit(1);
  }

  // std::cout << "Bus released by " << controller_id << std::endl;
  already_flush = false;
  already_busrd = false;
  just_released = true;
}

auto Bus::get_owner_id() -> std::optional<int> { return owner_id; }

auto Bus::reset() -> void { just_released = false; }