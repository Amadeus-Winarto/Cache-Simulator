#pragma once

#include <iostream>
#include <vector>

class StatisticsAccumulator {
private:
  std::vector<int> num_loads;
  std::vector<int> num_stores;
  std::vector<int> num_computes;

  std::vector<int> cycles_completion;
  std::vector<int> cycles_others;

public:
  StatisticsAccumulator(int num_cores);

  void register_num_loads(int processor_id, int num_loads);
  void register_num_stores(int processor_id, int num_stores);
  void register_num_computes(int processor_id, int num_computes);

  void on_run_end(int processor_id, int cycle_count);
  void on_compute_instr_end(int processor_id, int cycle_count);

  friend auto operator<<(std::ostream &os, const StatisticsAccumulator &p)
      -> std::ostream &;
};