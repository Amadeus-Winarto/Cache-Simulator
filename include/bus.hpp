#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

enum class BusRequestType { BusRd, BusRdX };

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
  bool request_ready{false};
  std::vector<BusRequest> request_queue;

  // Response Line(s)
  int num_responses{0};
  // They're not bool because vector<bool> is not thread-safe
  std::vector<int> response_valid_bits;
  std::vector<int> response_is_present_bits;
  std::vector<int> response_wait_bits;

  // Data line is not simulated since there's no actual data here in the
  // simulator

  Bus(int num_processors)
      : request_queue(1), response_valid_bits(num_processors, false),
        response_is_present_bits(num_processors, false),
        response_wait_bits(num_processors, false) {}
};
