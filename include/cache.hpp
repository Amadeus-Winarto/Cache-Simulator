#pragma once
#include "bus.hpp"
#include "trace.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <sys/types.h>
#include <thread>
#include <vector>

// Cache Parameters (in bits when appropriate)
static constexpr auto WORD_SIZE = 32; // 32 bits

struct ParsedAddress {
  uint32_t tag;
  uint32_t set_index;
  uint32_t offset;
  uint32_t address;
};
auto to_string(const ParsedAddress &parsed_address) -> std::string;

template <typename Status> struct CacheLine {
  uint32_t tag;
  const uint32_t set_index;
  int last_used;
  Status status;

  CacheLine(uint32_t set_index)
      : tag(0), set_index(set_index), last_used(0), status(Status::I) {}
};

template <typename Status>
auto to_string(const CacheLine<Status> &cache_line) -> std::string {
  return "CacheLine{set_index: " + std::to_string(cache_line.set_index) +
         ", tag: " + std::to_string(cache_line.tag) +
         ", last_used: " + std::to_string(cache_line.last_used) +
         ", status: " + to_string(cache_line.status) + "}";
}

template <typename Status>
auto to_string(std::shared_ptr<CacheLine<Status>> cache_line) -> std::string {
  if (!cache_line) {
    return "Invalid line!";
  } else {
    return to_string(*cache_line);
  };
}

template <typename Status> class CacheSet {
public:
  std::vector<std::shared_ptr<CacheLine<Status>>> lines;
  const uint32_t set_index;

  CacheSet(uint32_t set_index, int associativity)
      : lines(associativity), set_index(set_index) {
    for (auto &line : lines) {
      line = std::make_shared<CacheLine<Status>>(set_index);
    }
  }
};

template <typename Protocol> class Cache {
  using Status = typename Protocol::Status;

public:
  const int num_offset_bits;
  const int num_sets;
  const int num_set_index_bits;
  const int num_words_per_line;

  std::vector<std::shared_ptr<CacheSet<Status>>> sets;

  Cache(int cache_size, int associativity, int block_size)
      : num_offset_bits(std::log2(block_size)),
        num_sets((cache_size / associativity) /
                 block_size), // 64 Sets -> set_index goes from 0 to 63
        num_set_index_bits(std::log2(num_sets)), // 6 bits to address 64 sets
        num_words_per_line(block_size / (WORD_SIZE >> 3)), sets(num_sets) {

    for (int i = 0; i < num_sets; i++) {
      sets.at(i) = std::make_shared<CacheSet<Status>>(static_cast<uint32_t>(i),
                                                      associativity);
    }
  };

  auto read(uint32_t address) -> bool { return fetch(address); }

private:
  auto parse_address(uint32_t address) -> ParsedAddress {
    auto offset = address & ((1 << num_offset_bits) - 1);
    auto set_index =
        (address >> num_offset_bits) & ((1 << num_set_index_bits) - 1);
    auto tag = address >> (num_offset_bits + num_set_index_bits);
    return ParsedAddress{tag, set_index, offset};
  }

  auto fetch(uint32_t address) -> Status {
    auto parsed = parse_address(address);
    auto set_index = parsed.set_index;
    auto tag = parsed.tag;

    auto &set = sets.at(set_index);

    auto line = [&]() -> std::shared_ptr<CacheLine<Status>> {
      for (auto &line : set->lines) {
        if (line->tag == tag) {
          // Tag is in cache
          return line;
        }
      }
      return std::shared_ptr<CacheLine<Status>>{nullptr};
    }();

    if (line) {
      if (line->tag == tag && line->status != Status::I) {
        // Hit
        return true;
      }
    }
    return false; // Miss
  }
};
