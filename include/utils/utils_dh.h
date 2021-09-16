#ifndef CSO_UTILS_DH_H
#define CSO_UTILS_DH_H

#include <tuple>
#include "error/error.h"
#include "entity/array.hpp"
#include "entity/bigint.h"

class UtilsDH {
public:
    static BigInt generatePrivateKey();
    static std::tuple<Error, BigInt> calcPublicKey(const BigInt& gKey, const BigInt& nKey, const BigInt& privKey);
    static std::tuple<Error, Array<uint8_t>> calcSecretKey(const BigInt& nKey, const BigInt& clientPrivKey, const BigInt& serverPubKey);
};

#endif // !CSO_UTILS_DH_H