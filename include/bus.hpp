#pragma once
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <vector>

enum class BusRequestType { BusRd, BusRdX, BusUpd, Flush };

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
private:
  std::optional<int> owner_id;
  std::list<int> registration_queue;

public:
  bool already_flush = false;

  // Request Line
  std::optional<BusRequest> request_queue;

  // Response Line(s)
  std::vector<bool> response_completed_bits;
  std::vector<bool> response_is_present_bits;
  std::vector<bool> response_wait_bits;

  // Data line is not simulated since there's no actual data here in the
  // simulator

  Bus(int num_processors);

  auto acquire(int controller_id) -> int;
  void release(int controller_id);
  auto get_owner_id() -> std::optional<int>;
};
