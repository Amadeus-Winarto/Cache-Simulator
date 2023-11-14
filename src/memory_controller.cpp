#include "memory_controller.hpp"
#include "cache.hpp"
#include <cstdint>
#include <memory>
#include <sys/types.h>

auto MemoryController::set_delay(int delay) -> void {
#ifdef USE_WRITE_BUFFER
  this->delay = delay;
#endif
}

auto MemoryController::is_done() -> bool {
#ifdef USE_WRITE_BUFFER
  return write_buffer.is_empty();
#else
  return (!pending_write_back || pending_write_back.value() == 0);
#endif
}

auto MemoryController::run_once() -> void {
#ifdef USE_WRITE_BUFFER
  if (write_buffer.run_once()) {
    stats_accum->on_write_back();
  };
#else
  if (pending_write_back && pending_write_back.value() > 0) {
    pending_write_back = pending_write_back.value() - 1;
  }
#endif

  if (pending_data_read && pending_data_read.value() > 0) {
    pending_data_read = pending_data_read.value() - 1;
  }

  return;
}

auto MemoryController::write_back(uint32_t address) -> bool {
#ifdef USE_WRITE_BUFFER
  return write_back_with_write_buffer(address);
#else
  return simple_write_back(address);
#endif
}

auto MemoryController::read_data(uint32_t address) -> bool {
#ifdef USE_WRITE_BUFFER
  return read_data_with_write_buffer(address);
#else
  return simple_read_data(address);
#endif
}

#ifdef USE_WRITE_BUFFER
auto MemoryController::write_back_with_write_buffer(uint32_t address) -> bool {
  return write_buffer.add_to_queue(address);
}

auto MemoryController::read_data_with_write_buffer(uint32_t address) -> bool {
  if (!pending_data_read) {
    pending_data_read = (write_buffer.remove_if_present(address))
                            ? delay - 1
                            : MEMORY_MISS_PENALTY - 1;

    return false;
  } else if (pending_data_read && pending_data_read.value() == 0) {
    // Data read completed
    pending_data_read = std::nullopt;
    return true;
  } else {
    return false;
  }
}
#else
auto MemoryController::simple_write_back(uint32_t address) -> bool {
  if (!pending_write_back) {
    pending_write_back = MEMORY_MISS_PENALTY - 1;
    return false;
  } else if (pending_write_back && pending_write_back.value() == 0) {
    pending_write_back = std::nullopt;
    stats_accum->on_write_back();
    return true;
  } else {
    return false;
  }
}

auto MemoryController::simple_read_data(uint32_t address) -> bool {
  if (!pending_data_read) {
    pending_data_read = MEMORY_MISS_PENALTY - 1;
    return false;
  } else if (pending_data_read && pending_data_read.value() == 0) {
    // Data read completed
    pending_data_read = std::nullopt;
    return true;
  } else {
    return false;
  }
}
#endif
