#include "statistics.hpp"
#include "cache.hpp"
#include <algorithm>
#include <numeric>
#include <string>

StatisticsAccumulator::StatisticsAccumulator(int num_cores,
                                             std::vector<int> private_states,
                                             std::vector<int> public_states)
    : private_states(private_states), public_states(public_states),
      num_loads_instr(num_cores), num_stores_instr(num_cores),
      num_computes_instr(num_cores), num_read_hits(num_cores),
      num_write_hits(num_cores), num_computes(num_cores),
      cycles_completion(num_cores, -1), cycles_others(num_cores, -1),
      num_idles(num_cores), num_invalidates(num_cores),
      cache_accesses(num_cores) {}

void StatisticsAccumulator::register_num_loads(int processor_id,
                                               int num_instr) {
  num_loads_instr.at(processor_id) = num_instr;
}

void StatisticsAccumulator::register_num_stores(int processor_id,
                                                int num_instr) {
  num_stores_instr.at(processor_id) = num_instr;
}

void StatisticsAccumulator::register_num_computes(int processor_id,
                                                  int num_instr) {
  num_computes_instr.at(processor_id) = num_instr;
}

void StatisticsAccumulator::on_run_end(int processor_id, int cycle_count) {
  if (cycles_completion.at(processor_id) == -1) {
    cycles_completion.at(processor_id) = cycle_count;
  }
}

void StatisticsAccumulator::on_compute(int processor_id) {
  num_computes.at(processor_id) += 1;
}

void StatisticsAccumulator::on_read_hit(int processor_id, int state_id,
                                        int cycle_count) {
  num_read_hits.at(processor_id) += 1;
  cache_accesses.at(processor_id).at(0)[state_id] += 1;
}

void StatisticsAccumulator::on_write_hit(int processor_id, int state_id,
                                         int cycle_count) {
  num_write_hits.at(processor_id) += 1;
  cache_accesses.at(processor_id).at(1)[state_id] += 1;
}

void StatisticsAccumulator::on_idle(int processor_id, int cycle_count) {
  num_idles.at(processor_id) += 1;
}

void StatisticsAccumulator::on_write_back() { num_write_backs += 1; }

void StatisticsAccumulator::on_bus_traffic(int num_words) {
  num_bus_traffic += num_words;
}

void StatisticsAccumulator::on_invalidate(int processor_id) {
  num_invalidates.at(processor_id) += 1;
}

// void StatisticsAccumulator::on_cache_access(int processor_id, int state_id) {
//   cache_accesses.at(processor_id)[state_id] += 1;
// }

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

  os << "Number of Compute Cycles:\n";
  for (auto i = 0; i < p.cycles_others.size(); i++) {
    os << "\t Core " << i << ": " << p.num_computes.at(i) << "\n";
  }

  os << "Number of Loads/Stores Instructions:\n";
  for (auto i = 0; i < p.cycles_others.size(); i++) {
    os << "\t Core " << i << ": "
       << p.num_loads_instr.at(i) + p.num_stores_instr.at(i)
       << " instructions\n";
  }

  os << "Read Hits:\n";
  for (auto i = 0; i < p.num_read_hits.size(); i++) {
    auto hits = p.num_read_hits.at(i);
    auto hit_rate = hits / static_cast<float>(p.num_loads_instr.at(i)) * 100.0;
    os << "\t Core " << i << ": " << hits << " (" << hit_rate << "%)\n";
  }

  os << "Write Hits:\n";
  for (auto i = 0; i < p.num_write_hits.size(); i++) {
    auto hits = p.num_write_hits.at(i);
    auto hit_rate = hits / static_cast<float>(p.num_stores_instr.at(i)) * 100.0;
    os << "\t Core " << i << ": " << hits << " (" << hit_rate << "%)\n";
  }

  os << "Cache Misses:\n";
  for (auto i = 0; i < p.num_write_hits.size(); i++) {
    auto hits = p.num_write_hits.at(i) + p.num_read_hits.at(i);
    auto hit_rate =
        hits /
        static_cast<float>(p.num_loads_instr.at(i) + p.num_stores_instr.at(i)) *
        100.0;
    auto miss = p.num_loads_instr.at(i) + p.num_stores_instr.at(i) - hits;
    auto miss_rate = 100.0 - hit_rate;

    os << "\t Core " << i << ": " << miss << " (" << miss_rate << "%)\n";
  }

  os << "Instruction Per Cycle:\n";
  for (auto i = 0; i < p.cycles_completion.size(); i++) {
    auto instr = p.num_loads_instr.at(i) + p.num_stores_instr.at(i) +
                 p.num_computes_instr.at(i);
    auto ipc = instr / static_cast<float>(p.cycles_completion.at(i));
    os << "\t Core " << i << ": " << ipc << "\n";
  }

  os << "Idle Cycles:\n";
  for (auto i = 0; i < p.num_idles.size(); i++) {
    auto idle = p.num_idles.at(i);
    auto idle_rate =
        idle / static_cast<float>(p.cycles_completion.at(i)) * 100.0;
    os << "\t Core " << i << ": " << idle << " (" << idle_rate << "%)\n";
  }

  os << "Cache Hit Accesses:\n";
  for (auto i = 0; i < p.cache_accesses.size(); i++) {
    const auto &[reads, writes] = p.cache_accesses.at(i);

    auto public_accesses = 0;
    auto public_read = 0;
    auto public_write = 0;
    for (auto state_id : p.public_states) {
      auto read_accesses = reads.find(state_id);
      if (read_accesses != reads.end()) {
        public_accesses += read_accesses->second;
        public_read += read_accesses->second;
      }

      auto write_accesses = writes.find(state_id);
      if (write_accesses != writes.end()) {
        public_accesses += write_accesses->second;
        public_write += write_accesses->second;
      }
    }

    auto private_accesses = 0;
    auto private_read = 0;
    auto private_write = 0;
    for (auto state_id : p.private_states) {
      auto read_accesses = reads.find(state_id);
      if (read_accesses != reads.end()) {
        private_accesses += read_accesses->second;
        private_read += read_accesses->second;
      }

      auto write_accesses = writes.find(state_id);
      if (write_accesses != writes.end()) {
        private_accesses += write_accesses->second;
        private_write += write_accesses->second;
      }
    }

    auto private_read_pct =
        private_read / static_cast<float>(private_accesses) * 100.0;
    auto private_write_pct =
        private_write / static_cast<float>(private_accesses) * 100.0;

    auto public_read_pct =
        public_read / static_cast<float>(public_accesses) * 100.0;
    auto public_write_pct =
        public_write / static_cast<float>(public_accesses) * 100.0;

    os << "\t Core " << i << ": \n";
    os << "\t\t Public: " << public_accesses << " (R v. W: " << public_read_pct
       << "% v. " << public_write_pct << "%)\n";
    os << "\t\t Private: " << private_accesses
       << " (R v. W: " << private_read_pct << "% v. " << private_write_pct
       << "%)\n";
    os << "\t\t Public v. Private: " << public_accesses << " v. "
       << private_accesses << "\t("
       << (public_accesses * 100. /
           static_cast<float>(public_accesses + private_accesses))
       << "% v. "
       << (private_accesses * 100. /
           static_cast<float>(public_accesses + private_accesses))
       << ")\n";
  }

  os << "Cache Access (Among Hits):\n";
  for (auto i = 0; i < p.cache_accesses.size(); i++) {
    const auto &[reads, writes] = p.cache_accesses.at(i);

    os << "\tCore " << i << ":\n";
    os << "\t\tReads: \n";
    for (auto &[state_id, count] : reads) {
      os << "\t\t\tState " << state_id << ": " << count << " ("
         << count * 100. / p.num_read_hits.at(i) << "%)\n";
    }

    os << "\t\tWrites: \n";
    for (auto &[state_id, count] : writes) {
      os << "\t\t\tState " << state_id << ": " << count << " ("
         << count * 100. / p.num_write_hits.at(i) << "%)\n";
    }
  }

  os << "Bus Traffic: " << p.num_bus_traffic * (WORD_SIZE >> 3) << " bytes\n";

  os << "Write Backs: " << p.num_write_backs << "\n";

  os << "Num. Invalidates/Updates: \n";
  for (auto i = 0; i < p.num_invalidates.size(); i++) {
    os << "\t Core " << i << ": " << p.num_invalidates.at(i) << "\n";
  }

  os << "---------------------------------------------\n";
  return os;
}
