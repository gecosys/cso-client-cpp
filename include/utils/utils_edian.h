#ifndef CSO_UTILS_EDIAN_H
#define CSO_UTILS_EDIAN_H

#include <cstdint>

class UtilsEdian {
public:
    static bool isBigEndian() noexcept;

    static int16_t swap2Bytes(int16_t value) noexcept;
    static int32_t swap4Bytes(int32_t value) noexcept;
    static int64_t swap8Bytes(int64_t value) noexcept;
};

#endif // !CSO_UTILS_EDIAN_H