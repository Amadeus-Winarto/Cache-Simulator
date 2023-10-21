#include "cache.hpp"
#include "bus.hpp"

auto to_string(const ParsedAddress &parsed_address) -> std::string {
  return "ParsedAddress{tag: " + std::to_string(parsed_address.tag) +
         ", set_index: " + std::to_string(parsed_address.set_index) +
         ", offset: " + std::to_string(parsed_address.offset) + "}";
}

auto to_string(const BusRequest &request) -> std::string {
  return "BusRequest{type: " + std::to_string(static_cast<int>(request.type)) +
         ", address: " + std::to_string(request.address) +
         ", origin: " + std::to_string(request.controller_id) + "}";
}