#pragma once
#include "bus.hpp"

#include <memory>

constexpr auto MEMORY_MISS_PENALTY = 100;

class MemoryController {
private:
  std::optional<std::tuple<BusRequest, int>> pending_bus_request;

public:
  MemoryController();

  bool receive_bus_request(const BusRequest &request);
};