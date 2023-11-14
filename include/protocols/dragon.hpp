#pragma once

#include "protocol.hpp"

enum class DragonStatus { E = 3, Sm = 2, Sc = 1, M = 4, I = 0 };
auto to_string(const DragonStatus &status) -> std::string;

using DragonProtocol = Protocol<DragonStatus>;
