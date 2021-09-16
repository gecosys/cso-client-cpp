#include <random>
#include <openssl/err.h>
#include "utils/utils_dh.h"
#include "encryption/hasing.h"
#include "error/thirdparty.h"

BigInt UtilsDH::generatePrivateKey() {
    std::default_random_engine engine;
    std::uniform_int_distribution<int32_t> random(0, INT32_MAX);
    return BigInt{ random(engine) };
}

std::tuple<Error, BigInt> UtilsDH::calcPublicKey(const BigInt& gKey, const BigInt& nKey, const BigInt& privKey) {
    return gKey.powMod(privKey, nKey);
}

std::tuple<Error, Array<uint8_t>> UtilsDH::calcSecretKey(const BigInt& nKey, const BigInt& clientPrivKey, const BigInt& serverPubKey) {
    // Calculate secret key and convert to string
    std::string str;
    {
        Error err;
        BigInt secretKey;

        std::tie(err, secretKey) = serverPubKey.powMod(clientPrivKey, nKey);
        if (!err.nil()) {
            return { std::move(err), Array<uint8_t>{} };
        }

        std::tie(err, str) = secretKey.toString();
        if (!err.nil()) {
            return { std::move(err), Array<uint8_t>{} };
        }
    }

    // Hash SHA-256
    return Hashing::sha256(str.c_str(), str.length());
}