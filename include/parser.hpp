#include "argparse/argparse.hpp"

static const std::vector<std::string> SUPPORTED_PROTOCOLS = {"MESI", "Dragon"};

auto parser() -> argparse::ArgumentParser;