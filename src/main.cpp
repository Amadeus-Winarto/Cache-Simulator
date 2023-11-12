#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "memory_controller.hpp"
#include "parser.hpp"
#include "processor.hpp"
#include "statistics.hpp"
#include "trace.hpp"

#include "protocols/mesi.hpp"
#include "protocols/dragon.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

  auto stats_accum = std::make_shared<StatisticsAccumulator>(NUM_CORES);

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

  // Get Cache Protocol
  using MESIProcessor = Processor<MESIProtocol>;
  using MESICacheController = CacheController<MESIProtocol>;

  using DragonProcessor = Processor<DragonProtocol>;
  using DragonCacheController = CacheController<DragonProtocol>;

  // Create Bus
  auto bus = std::make_shared<Bus>(NUM_CORES);

  // Create Memory Controller
  auto memory_controller = std::make_shared<MemoryController>();

  // Create Cache Controllers
  auto cache_controllers =
      std::vector<std::shared_ptr<CacheController<MESIProtocol>>>{};
  cache_controllers.reserve(NUM_CORES);
  for (int i = 0; i < NUM_CORES; i++) {
    cache_controllers.emplace_back(
        std::make_shared<CacheController<MESIProtocol>>(
            i, cache_size, associativity, block_size, bus, memory_controller,
            stats_accum));
  }
  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [&cache_controllers](auto &&cc) {
                  cc->register_cache_controllers(cache_controllers);
                });

  const auto num_words_per_line =
      cache_controllers.at(0)->cache.num_words_per_line;
  memory_controller->set_delay(2 * num_words_per_line);

  // Create processors
  auto ready_cores = std::vector<std::shared_ptr<MESIProcessor>>{};
  for (int i = 0; i < NUM_CORES; i++) {
    ready_cores.emplace_back(std::make_shared<MESIProcessor>(
        i, traces.at(i), cache_controllers.at(i), stats_accum));
  }

  // Run simulation
  int cycle = -1;
  std::vector<int> cycle_completions(NUM_CORES, -1);

  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION BEGIN-------------------------"
      << std::endl;

  auto rng = std::default_random_engine{};
  while (std::any_of(ready_cores.begin(), ready_cores.end(),
                     [](auto &core) { return !core->is_done(); })) {
    cycle++;
    memory_controller->run_once();
    // std::shuffle(std::begin(ready_cores), std::end(ready_cores), rng);

    for (auto &core : ready_cores) {
      core->run_once(cycle);
      if (core->is_done()) {
        stats_accum->on_run_end(core->get_processor_id(), cycle);
      }
    }

    if (cycle % 1000000 == 0) {
      std::cout << "Cycle: " << cycle << std::endl;
      for (auto core : ready_cores) {
        std::cout << "\tCore " << core->get_processor_id() << ": "
                  << core->progress() << "%" << std::endl;
      }
    }
  }
  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION END-------------------------"
      << std::endl;

  std::cout << std::endl;
  std::cout << "-------------------------CACHE CONTENT-------------------------"
            << std::endl;
  for (const auto &core : ready_cores) {
    core->get_interesting_cache_lines();
  }
  std::cout << "-------------------------CACHE END-------------------------"
            << std::endl;

  std::cout << *stats_accum << std::endl;

  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [](auto &&cc) { cc->deregister_cache_controllers(); });

  return 0;
}
