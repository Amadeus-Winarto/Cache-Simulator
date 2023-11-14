#pragma once

#include "protocol.hpp"

enum class MESIStatus {
  M = 3,
  E = 2,
  S = 1,
  I = 0 // default
};
auto to_string(const MESIStatus &status) -> std::string;

using MESIProtocol = Protocol<MESIStatus>;
