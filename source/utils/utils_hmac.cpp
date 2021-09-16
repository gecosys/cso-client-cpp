#include <memory>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "message/define.h"
#include "utils/utils_hmac.h"
#include "utils/utils_define.h"
#include "error/thirdparty.h"

std::tuple<Error, Array<uint8_t>> UtilsHMAC::calcHMAC(const Array<uint8_t>& key, const Array<uint8_t>& data) {
    if (key.length() != LENGTH_KEY) {
        return std::make_tuple(
            Error{ GET_FUNC_NAME(), "Length of key must be 32" },
            Array<uint8_t>{}
        );
    }

    EVP_PKEY* pKey{};
    EVP_MD_CTX* ctx{};

    size_t lenOutput{};
    std::unique_ptr<uint8_t> output{};

    pKey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, key.get(), key.length());
    if (pKey == nullptr) {
        goto handleError;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        goto handleError;
    }

    if (!EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pKey)) {
        goto handleError;
    }

    if (!EVP_DigestSignUpdate(ctx, data.get(), data.length())) {
        goto handleError;
    }

    if (!EVP_DigestSignFinal(ctx, NULL, &lenOutput)) {
        goto handleError;
    }

    output.reset(new uint8_t[lenOutput]);
    if (!EVP_DigestSignFinal(ctx, output.get(), &lenOutput)) {
        goto handleError;
    }

    EVP_PKEY_free(pKey);
    EVP_MD_CTX_free(ctx);
    return {
        Error{},
        Array<uint8_t>{ output.release(), lenOutput }
    };

handleError:
    EVP_PKEY_free(pKey);
    EVP_MD_CTX_free(ctx);
    return {
        Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) },
        Array<uint8_t>{}
    };
}

std::tuple<Error, bool> UtilsHMAC::validateHMAC(const Array<uint8_t>& key, const Array<uint8_t>& data, const Array<uint8_t>& expectedHMAC) {
    Error err;
    Array<uint8_t> hmac;

    std::tie(err, hmac) = UtilsHMAC::calcHMAC(key, data);
    if (!err.nil()) {
        return { std::move(err), false };
    }

    return {
        Error{},
        CRYPTO_memcmp(hmac.get(), expectedHMAC.get(), hmac.length()) == 0
    };
}