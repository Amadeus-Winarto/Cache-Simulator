#include "bus.hpp"
#include "cache.hpp"
#include "cache_controller.hpp"
#include "memory_controller.hpp"
#include "parser.hpp"
#include "processor.hpp"
#include "trace.hpp"

#include "protocols/mesi.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
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
  std::cout << "Cache size: " << cache_size << std::endl;
  std::cout << "Associativity: " << associativity << std::endl;
  std::cout << "Block size: " << block_size << std::endl;

  auto traces = parse_traces(path_str);

  // Get Cache Protocol
  using MESIProcessor = Processor<MESIProtocol>;

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
            i, cache_size, associativity, block_size, bus, memory_controller));
  }
  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [&cache_controllers](auto &&cc) {
                  cc->register_cache_controllers(cache_controllers);
                });

  // Create processors
  auto ready_cores = std::list<std::shared_ptr<MESIProcessor>>{};
  for (int i = 0; i < NUM_CORES; i++) {
    ready_cores.emplace_back(std::make_shared<MESIProcessor>(
        i, traces.at(i), cache_controllers.at(i)));
  }
  auto expired_cores = std::list<std::shared_ptr<MESIProcessor>>{};

  // Run simulation
  int cycle = 0;

  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION BEGIN-------------------------"
      << std::endl;

  while (std::any_of(ready_cores.begin(), ready_cores.end(),
                     [](auto &core) { return !core->is_done(); })) {

    const int num_ready = ready_cores.size();
    int i = 0;
    while (i < num_ready) {
      auto core = ready_cores.front();
      ready_cores.pop_front();
      core->run_once(cycle);
      ready_cores.push_back(core);

      // if (instr) {
      //   ready_cores.push_back(core);
      // } else {
      //   expired_cores.push_back(core);
      // }
      i++;
    }

    // if (ready_cores.size() == 0) {
    //   // All cores have run. Re-schedule cores
    //   ready_cores = expired_cores;
    //   expired_cores.clear();
    // }

    if (cycle % 1000000 == 0) {
      std::cout << "Cycle: " << cycle << std::endl;
      for (auto core : ready_cores) {
        std::cout << "\tCore " << core->get_processor_id() << ": "
                  << core->progress() << "%" << std::endl;
      }
    }
    cycle++;
  }
  std::cout << std::endl;
  std::cout
      << "-------------------------SIMULATION END-------------------------"
      << std::endl;
  std::cout << "Simulation complete at cycle: " << cycle << std::endl;

  for (const auto &core : ready_cores) {
    core->get_interesting_cache_lines();
  }

  std::for_each(cache_controllers.begin(), cache_controllers.end(),
                [](auto &&cc) { cc->deregister_cache_controllers(); });

  return 0;
}
