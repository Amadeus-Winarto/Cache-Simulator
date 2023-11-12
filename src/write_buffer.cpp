#include "write_buffer.hpp"

auto MESIWriteBuffer::is_empty() -> bool { return queue.empty(); }

auto MESIWriteBuffer::run_once() -> bool {
  if (queue.empty()) {
    return false;
  }
  // Write front of queue to memory
  queue.front() = std::make_tuple(std::get<0>(queue.front()),
                                  std::get<1>(queue.front()) - 1);
  if (std::get<1>(queue.front()) == 0) {
    // If write is done, remove from queue
    queue.erase(queue.begin());
    return true;
  }
  return false;
}

auto MESIWriteBuffer::add_to_queue(ParsedAddress parsed_address) -> bool {
  if (capacity == -1 || queue.size() < capacity) {
    // Infinite capacity, or queue is not full
    queue.push_back(std::make_tuple(parsed_address, MEMORY_MISS_PENALTY));
    return true;
  } else {
    // Finite capacity, and queue is full
    return false;
  }
}

auto MESIWriteBuffer::remove_if_present(ParsedAddress parsed_address) -> bool {
  for (auto it = queue.begin(); it != queue.end(); it++) {
    if (std::get<0>(*it).address == parsed_address.address) {
      queue.erase(it);
      return true;
    }
  }
  return false;
}
