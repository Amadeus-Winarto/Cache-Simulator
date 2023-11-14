#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <vector>

class StatisticsAccumulator {
private:
  std::vector<int> num_loads_instr;
  std::vector<int> num_stores_instr;
  std::vector<int> num_computes_instr;

  std::vector<int> num_read_hits;
  std::vector<int> num_write_hits;
  std::vector<int> num_computes;

  std::vector<int> cycles_completion;
  std::vector<int> cycles_others;

  std::vector<int> num_idles;

  std::vector<int> num_invalidates;

  std::vector<std::array<std::map<int, int>, 2>> cache_accesses;
  std::optional<std::function<std::string(int)>> state_parser;

  int num_write_backs;

public:
  StatisticsAccumulator(int num_cores);

  void register_num_loads(int processor_id, int num_loads);
  void register_num_stores(int processor_id, int num_stores);
  void register_num_computes(int processor_id, int num_computes);

  void on_run_end(int processor_id, int cycle_count);
  void on_compute(int processor_id);

  void on_read_hit(int processor_id, int state_id, int cycle_count);
  void on_write_hit(int processor_id, int state_id, int cycle_count);

  void on_idle(int processor_id, int cycle_count);

  // void on_cache_access(int processor_id, int state_id);

  void on_invalidate(int processor_id);

  void register_state_parser(std::function<std::string(int)> parser) {
    state_parser = parser;
  }

  void on_write_back();

  friend auto operator<<(std::ostream &os, const StatisticsAccumulator &p)
      -> std::ostream &;
};