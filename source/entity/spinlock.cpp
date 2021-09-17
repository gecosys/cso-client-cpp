#include "entity/spinlock.h"
#include "utils/utils_general.hpp"

SpinLock::SpinLock() noexcept
    : value{ false } {}

SpinLock::~SpinLock() noexcept {}

void SpinLock::lock() noexcept {
    while (true) {
        // Optimistically assume the lock is free on the first try
        if (!this->value.exchange(true, std::memory_order_acquire)) {
            return;
        }
        // Wait for lock to be released without generating cache misses
        while (this->value.load(std::memory_order_relaxed)) {
            // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
            // hyper-threads
            PAUSE_CPU();
        }
    }
}

void SpinLock::unlock() noexcept {
    this->value.store(false, std::memory_order_release);
}