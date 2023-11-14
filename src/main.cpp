#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "memory_controller.hpp"
#include "parser.hpp"
#include "processor.hpp"
#include "protocols/moesi.hpp"
#include "statistics.hpp"
#include "trace.hpp"

#include "protocols/dragon.hpp"
#include "protocols/mesi.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

static constexpr int PRINT_INTERVAL = 1000000;
static constexpr char UNIT = 'M';

// Get Cache Protocol
using MESIProcessor = Processor<MESIProtocol>;
using MESICacheController = CacheController<MESIProtocol>;

using DragonProcessor = Processor<DragonProtocol>;
using DragonCacheController = CacheController<DragonProtocol>;

using MOESIProcessor = Processor<MOESIProtocol>;
using MOESICacheController = CacheController<MOESIProtocol>;

// Processor
using var_t = std::variant<
    std::tuple<std::vector<std::shared_ptr<MESICacheController>>,
               std::vector<std::shared_ptr<MESIProcessor>>>,
    std::tuple<std::vector<std::shared_ptr<DragonCacheController>>,
               std::vector<std::shared_ptr<DragonProcessor>>>,
    std::tuple<std::vector<std::shared_ptr<MOESICacheController>>,
               std::vector<std::shared_ptr<Processor<MOESIProtocol>>>>>;

template <typename Protocol>
auto build_cache_controllers(
    int cache_size, int associativity, int block_size, std::shared_ptr<Bus> bus,
    std::shared_ptr<MemoryController> memory_controller,
    std::shared_ptr<StatisticsAccumulator> stats_accum) {
  auto cache_controllers =
      std::vector<std::shared_ptr<CacheController<Protocol>>>{};
  cache_controllers.reserve(NUM_CORES);
  for (int i = 0; i < NUM_CORES; i++) {
    cache_controllers.emplace_back(std::make_shared<CacheController<Protocol>>(
        i, cache_size, associativity, block_size, bus, memory_controller,
        stats_accum));
  }
  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [&cache_controllers](auto &&cc) {
                  cc->register_cache_controllers(cache_controllers);
                });
  return cache_controllers;
}

template <typename Protocol>
auto build_cores(
    std::array<std::vector<Instruction>, NUM_CORES> traces,
    std::vector<std::shared_ptr<CacheController<Protocol>>> cache_controllers,
    std::shared_ptr<StatisticsAccumulator> stats_accum) {
  auto ready_cores = std::vector<std::shared_ptr<Processor<Protocol>>>{};
  for (int i = 0; i < NUM_CORES; i++) {
    ready_cores.emplace_back(std::make_shared<Processor<Protocol>>(
        i, traces.at(i), cache_controllers.at(i), stats_accum));
  }
  return ready_cores;
}

template <typename Protocol>
auto build_caches_and_cores(
    int cache_size, int associativity, int block_size, std::shared_ptr<Bus> bus,
    std::array<std::vector<Instruction>, NUM_CORES> traces,
    std::shared_ptr<MemoryController> memory_controller,
    std::shared_ptr<StatisticsAccumulator> stats_accum) {
  auto cache_controllers =
      build_cache_controllers<Protocol>(cache_size, associativity, block_size,
                                        bus, memory_controller, stats_accum);
  return std::make_tuple(
      cache_controllers,
      build_cores<Protocol>(traces, cache_controllers, stats_accum));
}

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
  std::cout << "Cache size: " << cache_size << " bytes" << std::endl;
  std::cout << "Associativity: " << associativity << std::endl;
  std::cout << "Block size: " << block_size << " bytes" << std::endl;

  auto private_states =
      protocol == SUPPORTED_PROTOCOLS.at(0)
          ? std::vector<int>{static_cast<int>(MESIStatus::M),
                             static_cast<int>(MESIStatus::E)}
      : protocol == SUPPORTED_PROTOCOLS.at(1)
          ? std::vector<int>{static_cast<int>(DragonStatus::M),
                             static_cast<int>(DragonStatus::E)}
          : std::vector<int>{static_cast<int>(MOESIStatus::M),
                             static_cast<int>(MOESIStatus::E)};

  auto public_states =
      protocol == SUPPORTED_PROTOCOLS.at(0)
          ? std::vector<int>{static_cast<int>(MESIStatus::S)}
      : protocol == SUPPORTED_PROTOCOLS.at(1)
          ? std::vector<int>{static_cast<int>(DragonStatus::Sm),
                             static_cast<int>(DragonStatus::Sc)}
          : std::vector<int>(static_cast<int>(MOESIStatus::O),
                             static_cast<int>(MOESIStatus::S));

  auto stats_accum = std::make_shared<StatisticsAccumulator>(
      NUM_CORES, private_states, public_states);

  auto traces = parse_traces(path_str);
  // Register traces information
  int i = 0;
  for (auto &trace : traces) {

    stats_accum->register_num_loads(
        i, std::count_if(trace.begin(), trace.end(), [](const auto &instr) {
          return instr.label == InstructionType::READ;
        }));

    stats_accum->register_num_stores(
        i, std::count_if(trace.begin(), trace.end(), [](const auto &instr) {
          return instr.label == InstructionType::WRITE;
        }));

    stats_accum->register_num_computes(
        i, std::count_if(trace.begin(), trace.end(), [](const auto &instr) {
          return instr.label == InstructionType::OTHER;
        }));
    i++;
  }

  // Create Bus
  auto bus = std::make_shared<Bus>(NUM_CORES);

  // Create Memory Controller
  auto memory_controller = std::make_shared<MemoryController>(stats_accum);

  // Create Cache Controllers and Processors
  auto variant_caches_and_cores =
      protocol == SUPPORTED_PROTOCOLS.at(0)
          ? var_t{build_caches_and_cores<MESIProtocol>(
                cache_size, associativity, block_size, bus, traces,
                memory_controller, stats_accum)}
      : protocol == SUPPORTED_PROTOCOLS.at(1)
          ? var_t{build_caches_and_cores<DragonProtocol>(
                cache_size, associativity, block_size, bus, traces,
                memory_controller, stats_accum)}
          : var_t{build_caches_and_cores<MOESIProtocol>(
                cache_size, associativity, block_size, bus, traces,
                memory_controller, stats_accum)};

  // Initialise memory controller delay
  const auto num_words_per_line = std::visit(
      [](auto &&arg) -> int {
        return std::get<0>(arg).at(0)->cache.num_words_per_line;
      },
      variant_caches_and_cores);
  memory_controller->set_delay(2 * num_words_per_line);

  // Run simulation
  int cycle = -1;
  std::vector<int> cycle_completions(NUM_CORES, -1);

  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION BEGIN-------------------------"
      << std::endl;

  auto rng = std::default_random_engine{};

  while (std::visit(
      [](auto &&arg) -> bool {
        auto cores = std::get<1>(arg);
        return std::any_of(cores.begin(), cores.end(),
                           [](auto &core) { return !core->is_done(); });
      },
      variant_caches_and_cores)) {
    cycle++;
    memory_controller->run_once();
    bus->reset();

    // Run each core once
    std::visit(
        [cycle, stats_accum](auto &&arg) {
          auto cores = std::get<1>(arg);
          std::for_each(
              cores.begin(), cores.end(), [cycle, stats_accum](auto &core) {
                core->run_once(cycle);
                if (core->is_done()) {
                  stats_accum->on_run_end(core->get_processor_id(), cycle);
                }
              });
        },
        variant_caches_and_cores);

    if (cycle % PRINT_INTERVAL == 0) {
      std::cout << "Cycle: " << (cycle / PRINT_INTERVAL) << UNIT << std::endl;
      std::visit(
          [](auto &&arg) {
            auto cores = std::get<1>(arg);
            std::for_each(cores.begin(), cores.end(), [](auto &core) {
              std::cout << "\tCore " << core->get_processor_id() << ": "
                        << core->progress() << "%" << std::endl;
            });
          },
          variant_caches_and_cores);
    }
  }
  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION END-------------------------"
      << std::endl;

  std::cout << std::endl;
  std::cout << "-------------------------CACHE CONTENT-------------------------"
            << std::endl;
  std::visit(
      [](auto &&arg) {
        auto cores = std::get<1>(arg);
        std::for_each(cores.begin(), cores.end(),
                      [](auto &&core) { core->get_interesting_cache_lines(); });
      },
      variant_caches_and_cores);
  std::cout << "-------------------------CACHE END-------------------------"
            << std::endl;

  std::cout << *stats_accum << std::endl;

  std::visit(
      [](auto &&arg) {
        auto cache_controllers = std::get<0>(arg);
        std::for_each(cache_controllers.begin(), cache_controllers.end(),
                      [](auto &&cc) { cc->deregister_cache_controllers(); });
      },
      variant_caches_and_cores);

  return 0;
}
