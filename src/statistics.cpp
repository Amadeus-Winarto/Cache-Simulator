#include "statistics.hpp"
#include <algorithm>

StatisticsAccumulator::StatisticsAccumulator(int num_cores)
    : num_loads(num_cores), num_stores(num_cores), num_computes(num_cores),
      num_read_hits(num_cores), num_write_hits(num_cores),
      cycles_completion(num_cores, -1), cycles_others(num_cores, -1) {}

void StatisticsAccumulator::register_num_loads(int processor_id,
                                               int num_instr) {
  num_loads.at(processor_id) = num_instr;
}

void StatisticsAccumulator::register_num_stores(int processor_id,
                                                int num_instr) {
  num_stores.at(processor_id) = num_instr;
}

void StatisticsAccumulator::register_num_computes(int processor_id,
                                                  int num_instr) {
  num_computes.at(processor_id) = num_instr;
}

void StatisticsAccumulator::on_run_end(int processor_id, int cycle_count) {
  if (cycles_completion.at(processor_id) == -1) {
    cycles_completion.at(processor_id) = cycle_count;
  }
}

void StatisticsAccumulator::on_compute_instr_end(int processor_id,
                                                 int cycle_count) {
  if (cycles_others.at(processor_id) == -1) {
    cycles_others.at(processor_id) = cycle_count;
  }
}

void StatisticsAccumulator::on_read_hit(int processor_id, int cycle_count) {
  num_read_hits.at(processor_id) += 1;
}

void StatisticsAccumulator::on_write_hit(int processor_id, int cycle_count) {
  num_write_hits.at(processor_id) += 1;
}

auto operator<<(std::ostream &os, const StatisticsAccumulator &p)
    -> std::ostream & {
  const auto max_cycle =
      std::max_element(p.cycles_completion.begin(), p.cycles_completion.end());

  os << "-------------STATISTICS----------------------\n";
  os << "Overall Execution Cycle: " << *max_cycle << "\n";
  for (auto i = 0; i < p.cycles_completion.size(); i++) {
    os << "\t Core " << i
       << " completes at cycle: " << p.cycles_completion.at(i) << "\n";
  }

  os << "Compute cycles per core:\n";
  for (auto i = 0; i < p.cycles_others.size(); i++) {
    os << "\t Core " << i << " completes at cycle: " << p.cycles_others.at(i)
       << "\n";
  }

  os << "Number of Loads/Stores:\n";
  for (auto i = 0; i < p.cycles_others.size(); i++) {
    os << "\t Core " << i << ": " << p.num_loads.at(i) + p.num_stores.at(i)
       << " instructions\n";
  }

  os << "Read Hits:\n";
  for (auto i = 0; i < p.num_read_hits.size(); i++) {
    auto hits = p.num_read_hits.at(i);
    auto hit_rate = hits / static_cast<float>(p.num_loads.at(i)) * 100.0;
    os << "\t Core " << i << ": " << hits << " (" << hit_rate << "%)\n";
  }

  os << "Write Hits:\n";
  for (auto i = 0; i < p.num_write_hits.size(); i++) {
    auto hits = p.num_write_hits.at(i);
    auto hit_rate = hits / static_cast<float>(p.num_stores.at(i)) * 100.0;
    os << "\t Core " << i << ": " << hits << " (" << hit_rate << "%)\n";
  }

  os << "---------------------------------------------\n";
  return os;
}
