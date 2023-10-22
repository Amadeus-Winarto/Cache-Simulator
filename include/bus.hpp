#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

enum class BusRequestType { BusRd, BusRdX, Flush };

struct BusRequest {
  BusRequestType type;
  uint32_t address;
  int controller_id;
};

auto to_string(const BusRequest &request) -> std::string;

/**
 * @brief Defines a bus that connects all the caches.
 *
 */
class Bus {
public:
  std::optional<int> owner_id;

  // Request Line
  std::optional<BusRequest> request_queue;

  // Response Line(s)
  std::vector<bool> response_valid_bits;
  std::vector<bool> response_is_present_bits;
  std::vector<bool> response_wait_bits;

  // Data line is not simulated since there's no actual data here in the
  // simulator

  Bus(int num_processors)
      : response_valid_bits(num_processors, false),
        response_is_present_bits(num_processors, false),
        response_wait_bits(num_processors, false) {}
};
