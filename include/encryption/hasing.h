#ifndef CSO_ENCRYPTION_HASHING_H
#define CSO_ENCRYPTION_HASHING_H

#include <tuple>
#include "error/error.h"
#include "entity/array.hpp"

class Hashing {
public:
    static std::tuple<Error, Array<uint8_t>> sha256(const void* data, size_t len);
};

#endif // !CSO_ENCRYPTION_HASHING_H
