#ifndef CSO_ENTITY_BIGINT_H
#define CSO_ENTITY_BIGINT_H

#include <tuple>
#include <string>
#include <openssl/bn.h>

#include "error/error.h"

class BigInt {
private:
    BIGNUM* bn;

public:
    BigInt() noexcept;
    BigInt(int64_t n);
    BigInt(const BigInt& other);
    BigInt(BigInt&& other) noexcept;
    ~BigInt() noexcept;

    BigInt& operator=(const BigInt& other);
    BigInt& operator=(BigInt&& other) noexcept;

    Error setString(const std::string& str, int8_t radix = 10);
    std::tuple<Error, std::string> toString(int8_t radix = 10) const;
    std::tuple<Error, BigInt> powMod(const BigInt& power, const BigInt& modulus) const;
};

#endif // !CSO_ENTITY_BIGINT_H