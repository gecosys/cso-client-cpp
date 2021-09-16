#include <openssl/sha.h>
#include <openssl/err.h>
#include "error/thirdparty.h"
#include "encryption/hasing.h"
#include "utils/utils_define.h"

std::tuple<Error, Array<uint8_t>> Hashing::sha256(const void* data, size_t len) {
    SHA256_CTX ctx;
    Array<uint8_t> output{ 32 };

    if (!SHA256_Init(&ctx)) {
        goto handleError;
    }

    if (!SHA256_Update(&ctx, data, len)) {
        goto handleError;
    }

    if (!SHA256_Final(output.get(), &ctx)) {
        goto handleError;
    }

    return { Error{}, std::move(output) };

handleError:
    return {
        Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) },
        Array<uint8_t>{}
    };
}