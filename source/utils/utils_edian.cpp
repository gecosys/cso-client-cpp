#include "utils/utils_edian.h"

bool UtilsEdian::isBigEndian() noexcept {
    short int number = 0x1U;
    char* numPtr = (char*)&number;
    return (numPtr[0] != 1);
}

int16_t UtilsEdian::swap2Bytes(int16_t value) noexcept {
    return ((value >> 8U) & 0xFFU) |
           ((value << 8U) & 0xFF00U);
}

int32_t UtilsEdian::swap4Bytes(int32_t value) noexcept {
    return (value >> 24U) |
           ((value >> 8U) & 0xFF00U) |
           ((value << 8U) & 0xFF0000U) |
           ((value << 24U) & 0xFF000000U);
}

int64_t UtilsEdian::swap8Bytes(int64_t value) noexcept {
    return (value >> 56ULL) |
           ((value >> 40ULL) & 0xFF00ULL) |
           ((value >> 24ULL) & 0xFF0000ULL) |
           ((value >> 8ULL) & 0xFF000000ULL) |
           ((value << 8ULL) & 0xFF00000000ULL) |
           ((value << 24ULL) & 0xFF0000000000ULL) |
           ((value << 40ULL) & 0xFF000000000000ULL) |
           ((value << 56ULL) & 0xFF00000000000000ULL);
}