#include "memory_controller.hpp"
#include <memory>

MemoryController::MemoryController(){};

auto MemoryController::receive_bus_request(const BusRequest &request) -> bool {
  if (pending_bus_request) {
    auto [request, cycles_left] = pending_bus_request.value();
    if (--cycles_left > 0) {
      pending_bus_request = std::make_tuple(request, cycles_left);
      return false;
    } else {
      pending_bus_request = std::nullopt;
      return true;
    }
  } else {
    pending_bus_request = std::make_tuple(request, MEMORY_MISS_PENALTY - 1);
    return false;
  }
}