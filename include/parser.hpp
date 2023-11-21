#include "argparse/argparse.hpp"

static const std::vector<std::string> SUPPORTED_PROTOCOLS = {"MESI", "Dragon",
                                                             "MOESI", "MESIF"};

auto parser() -> argparse::ArgumentParser;