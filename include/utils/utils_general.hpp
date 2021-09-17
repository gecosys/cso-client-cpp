#ifndef CSO_UTILS_GENERAL_H
#define CSO_UTILS_GENERAL_H

#include <memory>
#include <string>
#include <stdexcept>

#ifdef __cpp_lib_hardware_interference_size // From C++17
#include <new>
static constexpr size_t CPU_CACHE_LINE_SIZE = std::hardware_destructive_interference_size; // More exactly
#else
static constexpr size_t CPU_CACHE_LINE_SIZE = 64; // x86 CPU and some ARM CPU got a cache line size is 64 bytes
#endif

#if defined(__GNUC__) || defined(__clang__)

static std::string getMethodName(const std::string& name) noexcept {
    size_t begin = name.find_last_of("::");
    for (; begin > 0 && name[begin] != ' '; --begin);
    begin++;
    return name.substr(begin, name.rfind("(") - begin);
}

#define PAUSE_CPU() __builtin_ia32_pause()
#define GET_FUNC_NAME() getMethodName(__PRETTY_FUNCTION__)

#elif defined(_MSC_VER)
#include <emmintrin.h>

#define PAUSE_CPU() _mm_pause()
#define GET_FUNC_NAME() __FUNCTION__
#endif

template<typename ... Args>
static std::string format(const std::string& format, Args&& ... args) {
    size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1ULL; // Extra space for '\0'
    if (size <= 0) {
        throw "Error during formatting.";
    }
    std::unique_ptr<char> buf{ new char[size] };
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string{ buf.get(), buf.get() + size - 1 }; // We don't want the '\0' inside
}

#endif // !CSO_UTILS_GENERAL_H
