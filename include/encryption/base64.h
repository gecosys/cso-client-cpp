#ifndef CSO_ENCRYPTION_BASE64_H
#define CSO_ENCRYPTION_BASE64_H

#include <string>
#include "entity/array.hpp"

class Base64 {
public:
    static std::string encode(const Array<uint8_t>& data);
    static Array<uint8_t> decode(const std::string& data);
};

#endif // !CSO_ENCRYPTION_BASE64_H