#include "argparse/argparse.hpp"
#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "processor.hpp"
#include "protocols/mesi.hpp"
#include "trace.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

auto parser() -> argparse::ArgumentParser;

int main(int argc, char **argv) {
  auto program = parser();
  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  const auto protocol = program.get<std::string>("protocol");
  const auto path_str = program.get<std::string>("input_file");
  const auto cache_size = program.get<int>("cache_size");
  const auto associativity = program.get<int>("associativity");
  const auto block_size = program.get<int>("block_size");

  std::cout << "Protocol: " << protocol << std::endl;
  std::cout << "Input file: " << path_str << std::endl;
  std::cout << "Cache size: " << cache_size << std::endl;
  std::cout << "Associativity: " << associativity << std::endl;
  std::cout << "Block size: " << block_size << std::endl;

  auto traces = parse_traces(path_str);

  // Get Cache Protocol
  using MESIProcessor = Processor<MESIProtocol>;

  // Create Bus
  auto bus = std::make_shared<Bus>(NUM_CORES);

  // Create Cache Controllers
  auto cache_controllers =
      std::vector<std::shared_ptr<CacheController<MESIProtocol>>>{};
  cache_controllers.reserve(NUM_CORES);
  for (int i = 0; i < NUM_CORES; i++) {
    cache_controllers.emplace_back(
        std::make_shared<CacheController<MESIProtocol>>(
            i, cache_size, associativity, block_size, bus));
  }
  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [&cache_controllers](auto &&cc) {
                  cc->register_cache_controllers(cache_controllers);
                });

  // Create processors
  auto cores = std::vector<std::shared_ptr<MESIProcessor>>{};
  cores.reserve(NUM_CORES);
  for (int i = 0; i < NUM_CORES; i++) {
    cores.emplace_back(std::make_shared<MESIProcessor>(
        i, traces.at(i), cache_controllers.at(i)));
  }

  // Run simulation
  int cycle = 0;

  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION BEGIN-------------------------"
      << std::endl;
  while (std::any_of(cores.begin(), cores.end(),
                     [](auto &core) { return !core->is_done(); })) {
    for (auto &core : cores) {
      core->run_once(cycle);
    }

    cycle++;
  }
  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION END-------------------------"
      << std::endl;
  std::cout << "Simulation complete at cycle: " << cycle << std::endl;

  for (const auto &core : cores) {
    core->get_interesting_cache_lines();
  }

  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [](auto &&cc) { cc->deregister_cache_controllers(); });

  return 0;
}

auto parser() -> argparse::ArgumentParser {
  argparse::ArgumentParser program{"Cache Simulator"};
  program.add_argument("protocol")
      .help("Cache coherence protocol to use")
      .action([](const std::string &value) {
        static const std::vector<std::string> SUPPORTED_PROTOCOLS = {"MESI"};
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
      .help("Cache size (bytes)");

  program.add_argument("--associativity")
      .default_value(2)
      .help("Associativity of the cache");

  program.add_argument("--block_size")
      .default_value(32)
      .help("Block size (bytes)");
  return program;
}