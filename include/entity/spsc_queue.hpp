#ifndef CSO_ENTITY_SPSC_QUEUE_HPP
#define CSO_ENTITY_SPSC_QUEUE_HPP

#if defined(clang) || defined(GNUC) // clang and GCC
#include <stdlib.h> // posix_memalign
#elif defined(_MSC_VER) // MSVC
#include <malloc.h> // _aligned_malloc
#endif

#include <thread>
#include <tuple>
#include <atomic>
#include <limits>
#include <cassert>
#include <type_traits>

#include "utils/utils_general.hpp"

template<typename Type, bool UsingCPUCacheLine>
class SPSCQueue {
private:
    class Slot {
    private:
        struct AtomNormal {
            std::atomic<size_t> value;

            AtomNormal(const size_t v)
                : value{ v } {}

            size_t load(const std::memory_order order) const noexcept {
                return this->value.load(order);
            }

            void store(const size_t v, const std::memory_order order) noexcept {
                this->value.store(v, order);
            }
        };

        struct AtomCPUCacheLine {
            alignas(CPU_CACHE_LINE_SIZE) std::atomic<size_t> value;

            AtomCPUCacheLine(const size_t v)
                : value{ v } {}

            size_t load(const std::memory_order order) const noexcept {
                return this->value.load(order);
            }

            void store(const size_t v, const std::memory_order order) noexcept {
                this->value.store(v, order);
            }
        };

    public:
        using Atom = typename std::conditional<UsingCPUCacheLine, AtomCPUCacheLine, AtomNormal>::type;
        using Storage = typename std::aligned_storage<sizeof(Type), alignof(Type)>::type;
        Atom turn{ 0 };
        Storage storage{};

    public:
        ~Slot() noexcept {
            // "turn" is odd number that slot is written (or has data)
            if (this->turn.load(std::memory_order_acquire) & 1) {
                destroy();
            }
        }

        template<typename... Args>
        void construct(Args &&... args) noexcept {
            new (&this->storage) Type(std::forward<Args>(args)...);
        }

        void destroy() noexcept {
            reinterpret_cast<Type*>(std::addressof(this->storage))->~Type();
        }

        Type&& get() noexcept {
            return std::move(*(reinterpret_cast<Type*>(std::addressof(this->storage))));
        }
    };

#if defined(cpp_aligned_new)
    template <typename T> using AlignedAllocator = std::allocator<T>;
#else
    template <typename T>
    struct AlignedAllocator {
        T* allocate(std::size_t n) {
            if (n > SIZE_MAX / sizeof(T)) {
                throw "Size is so large";
            }
#ifdef _MSC_VER
            auto* ptr = static_cast<T*>(_aligned_malloc(sizeof(T) * n, alignof(T)));
            if (ptr == nullptr) {
                throw "Memory allocation is failed";
            }
#else
            T* ptr;
            if (posix_memalign(reinterpret_cast<void**>(&ptr), alignof(T),
                sizeof(T) * n) != 0) {
                throw "Memory allocation is failed";
            }
#endif
            return ptr;
        }

        void deallocate(T* p) {
#ifdef _MSC_VER
            _aligned_free(p);
#else
            free(p);
#endif
        }
    };
#endif

private:
    using Allocator = AlignedAllocator<Slot>;
    size_t capacity;
    Slot* slots;
    Allocator allocator;
    alignas(CPU_CACHE_LINE_SIZE) size_t head;
    alignas(CPU_CACHE_LINE_SIZE) size_t tail;

private:
    // If run on 32-bits platform, "size_t" is uint32_t.
    // If run on 64-bits platform, "size_t" is uint64_t.
    constexpr size_t roundupToPowerOf2(size_t n) const noexcept {
        --n;
        n |= n >> 1U;
        n |= n >> 2U;
        n |= n >> 4U;
        n |= n >> 8U;
        n |= n >> 16U;
        if (sizeof(size_t) == 8) {
            n |= n >> 32U;
        }
        return ++n;
    }

    constexpr size_t idx(size_t i) const noexcept { return i & (this->capacity - 1U); }
    constexpr size_t turn(size_t i) const noexcept { return i / this->capacity; }

    void waitSlotReady(size_t turn, Slot& slot) noexcept {
        uint8_t retry = 10;
        while (turn != slot.turn.load(std::memory_order_acquire)) {
            if (--retry) {
                // Slowing down the “spin-wait” with the PAUSE instruction
                PAUSE_CPU();
            }
            else {
                // Sleep current thread and switch to another thread
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                retry = 10;
            }
        }
    }

public:
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue(const SPSCQueue& other) = delete;

    SPSCQueue& operator=(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;

    explicit SPSCQueue(const size_t capacity)
        : capacity{ 0 },
          slots{ nullptr },
          allocator{},
          head{ 0 },
          tail{ 0 } {

        static_assert(
            std::is_nothrow_default_constructible<Type>::value &&
            (std::is_nothrow_move_constructible<Type>::value ||
                std::is_nothrow_copy_constructible<Type>::value),
            "Type must have noexcept default constructor and (move constructor and move assigment) or copy constructor"
        );

        static_assert(
            std::is_nothrow_destructible<Type>::value,
            "Type must have noexcept destructor"
        );

        static_assert(
            !UsingCPUCacheLine || alignof(Slot) % CPU_CACHE_LINE_SIZE == 0,
            "Type size must be a multiple of CPU cache line when using CPU cache line mode"
        );

        if (capacity == 0) {
            throw "Capacity must be larger than 0";
        }

        this->capacity = roundupToPowerOf2(capacity);
        this->slots = this->allocator.allocate(this->capacity);

        // Verify alignment
        if (reinterpret_cast<size_t>(this->slots) % alignof(Slot) != 0) {
            this->allocator.deallocate(this->slots);
            throw "Memory allocation is failed";
        }

        // Initialize items
        for (size_t i = 0; i < this->capacity; ++i) {
            new (&this->slots[i]) Slot();
        }
    }

    ~SPSCQueue() noexcept {
        for (size_t i = 0; i < this->capacity; ++i) {
            this->slots[i].~Slot();
        }
        this->allocator.deallocate(this->slots);
    }

    //======
    // Push
    //======

    template <typename... Args>
    bool try_push(Args &&... args) noexcept {
        auto tail = this->tail;
        auto turn = this->turn(tail) << 1U;
        auto& slot = this->slots[idx(tail)];

        if (turn != slot.turn.load(std::memory_order_acquire)) {
            return false;
        }

        this->tail++;
        slot.construct(std::forward<Args>(args)...);
        slot.turn.store(turn + 1U, std::memory_order_release);
        return true;
    }

    template <typename... Args>
    void push(Args &&... args) noexcept {
        auto tail = this->tail++;
        auto turn = this->turn(tail) << 1U;
        auto& slot = this->slots[idx(tail)];

        while (turn != slot.turn.load(std::memory_order_acquire)) {
            waitSlotReady(turn, slot);
        }
        slot.construct(std::forward<Args>(args)...);
        slot.turn.store(turn + 1U, std::memory_order_release);
    }

    //=====
    // Pop
    //=====

    std::tuple<bool, Type> try_pop() noexcept {
        auto head = this->head;
        auto turn = (this->turn(head) << 1U) + 1U;
        auto& slot = this->slots[idx(head)];

        if (turn != slot.turn.load(std::memory_order_acquire)) {
            return { false, Type{} };
        }

        this->head++;
        // Get result & Update slot
        std::tuple<bool, Type> result{ true, slot.get() };
        slot.destroy();
        slot.turn.store(turn + 1U, std::memory_order_release);
        return result;
    }

    Type pop() noexcept {
        auto head = this->head++;
        auto turn = (this->turn(head) << 1U) + 1U;
        auto& slot = this->slots[idx(head)];

        while (turn != slot.turn.load(std::memory_order_acquire)) {
            waitSlotReady(turn, slot);
        }

        // Get result & Update slot
        Type result{ slot.get() };
        slot.destroy();
        slot.turn.store(turn + 1U, std::memory_order_release);
        return result;
    }
};

#endif // !CSO_ENTITY_SPSC_QUEUE_HPP