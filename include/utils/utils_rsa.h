#ifndef CSO_UTILS_RSA_H
#define CSO_UTILS_RSA_H

#include <tuple>
#include <string>
#include <openssl/evp.h>
#include "error/error.h"
#include "entity/array.hpp"

class UtilsRSA {
private:
    static std::tuple<Error, EVP_PKEY*> parsePublicKey(const std::string& publicKey);

public:
    static std::tuple<Error, bool> verifySignature(const std::string& publicKey, const Array<uint8_t>& signature, const Array<uint8_t>& data);
};

#endif // !CSO_UTILS_RSA_H