#pragma once

#include "protocol.hpp"

enum class MOESIStatus {
  M = 4,
  O = 3,
  E = 2,
  S = 1,
  I = 0 // default
};
auto to_string(const MOESIStatus &status) -> std::string;

using MOESIProtocol = Protocol<MOESIStatus>;
