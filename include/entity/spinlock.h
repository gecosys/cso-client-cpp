#ifndef CSO_ENTITY_SPINLOCK_H
#define CSO_ENTITY_SPINLOCK_H

#include <atomic>
#include "utils/utils_define.h"

class SpinLock {
private:
    alignas(CPU_CACHE_LINE_SIZE) std::atomic<bool> value;

public:
    SpinLock() noexcept;
    SpinLock(SpinLock&& other) = delete;
    SpinLock(const SpinLock& other) = delete;
    ~SpinLock() noexcept;

    SpinLock& operator=(SpinLock&& other) = delete;
    SpinLock& operator=(const SpinLock& other) = delete;

    void lock() noexcept;
    void unlock() noexcept;
};

#endif // !CSO_ENTITY_SPINLOCK_H