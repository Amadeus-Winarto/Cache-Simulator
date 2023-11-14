#include "parser.hpp"
#include "argparse/argparse.hpp"

#include <filesystem>
#include <sstream>

auto parser() -> argparse::ArgumentParser {
  argparse::ArgumentParser program{"Cache Simulator"};

  std::stringstream ss;
  ss << "Cache coherence protocol to use. One of: [";
  for (auto it = SUPPORTED_PROTOCOLS.begin(); it != SUPPORTED_PROTOCOLS.end();
       it++) {
    ss << *it;
    if (it != SUPPORTED_PROTOCOLS.end() - 1) {
      ss << ", ";
    }
  }
  ss << "]";

  program.add_argument("protocol")
      .help(ss.str())
      .action([](const std::string &value) {
        if (std::find(SUPPORTED_PROTOCOLS.begin(), SUPPORTED_PROTOCOLS.end(),
                      value) != SUPPORTED_PROTOCOLS.end()) {
          return value;
        }
        std::stringstream ss;
        ss << "Invalid protocol: " << value << std::endl;
        throw std::runtime_error{ss.str()};
      });

  program.add_argument("input_file")
      .help("Input benchmark name. Must be in the current directory")
      .action([](const std::string &value) {
        auto dirpath = std::filesystem::path{value};
        if (!std::filesystem::exists(dirpath)) {
          std::stringstream ss;
          ss << "Given path: " << dirpath << " does not exist!" << std::endl;
          throw std::runtime_error{ss.str()};
        }

        if (!std::filesystem::is_directory(dirpath)) {
          std::stringstream ss;
          ss << "Given path: " << dirpath << " is not a directory!"
             << std::endl;
          throw std::runtime_error{ss.str()};
        }
        return value;
      });

  program.add_argument("--cache_size")
      .default_value(4096)
      .scan<'d', int>()
      .help("Cache size (bytes)");

  program.add_argument("--associativity")
      .default_value(2)
      .scan<'d', int>()
      .help("Associativity of the cache");

  program.add_argument("--block_size")
      .default_value(32)
      .scan<'d', int>()
      .help("Block size (bytes)");
  return program;
}