#ifndef CSO_UTILS_DEFINE_H
#define CSO_UTILS_DEFINE_H

//=========================
// Define cache line's size
//=========================
#ifdef __cpp_lib_hardware_interference_size // From C++17
#include <new>
static constexpr size_t CPU_CACHE_LINE_SIZE = std::hardware_destructive_interference_size; // More exactly
#else
static constexpr size_t CPU_CACHE_LINE_SIZE = 64; // x86 CPU and some ARM CPU got a cache line size is 64 bytes
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PAUSE_CPU() __builtin_ia32_pause()
#elif defined(_MSC_VER)
#include <emmintrin.h>
#define PAUSE_CPU() _mm_pause()
#endif

#define GET_FUNC_NAME() __FUNCTION__

#endif // !CSO_UTILS_DEFINE_H
