#pragma once

#include "protocol.hpp"

enum class MESIFStatus {
  M = 4,
  E = 3,
  S = 2,
  F = 1,
  I = 0 // default
};
auto to_string(const MESIFStatus &status) -> std::string;

using MESIFProtocol = Protocol<MESIFStatus>;
