#include <openssl/evp.h>
#include <openssl/err.h>
#include "message/define.h"
#include "utils/utils_aes.h"
#include "utils/utils_general.hpp"
#include "error/thirdparty.h"

std::tuple<Error, Array<uint8_t>, Array<uint8_t>, Array<uint8_t>> UtilsAES::encrypt(const Array<uint8_t>& key, const Array<uint8_t>& input, const Array<uint8_t>& aad) {
    if (key.length() != LENGTH_KEY) {
        return {
            Error{ GET_FUNC_NAME(), "Length of key must be 32" },
            Array<uint8_t>{},
            Array<uint8_t>{},
            Array<uint8_t>{}
        };
    }

    Array<uint8_t> iv;
    Array<uint8_t> tag;
    Array<uint8_t> output;
    EVP_CIPHER_CTX* ctx;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        goto handleError;
    }

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        goto handleError;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, LENGTH_IV, nullptr)) {
        goto handleError;
    }

    iv.reset(LENGTH_IV);
    if (!EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.get(), iv.get())) {
        goto handleError;
    }

    int32_t lenOutput;
    if (!EVP_EncryptUpdate(ctx, nullptr, &lenOutput, aad.get(), aad.length())) {
        goto handleError;
    }

    output.reset(input.length());
    if (!EVP_EncryptUpdate(ctx, output.get(), &lenOutput, input.get(), input.length())) {
        goto handleError;
    }

    if (!EVP_EncryptFinal_ex(ctx, output.get() + lenOutput, &lenOutput)) {
        goto handleError;
    }

    tag.reset(LENGTH_AUTHEN_TAG);
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, LENGTH_AUTHEN_TAG, tag.get())) {
        goto handleError;
    }

    EVP_CIPHER_CTX_free(ctx);
    return {
        Error{},
        std::move(iv),
        std::move(tag),
        std::move(output)
    };

handleError:
    EVP_CIPHER_CTX_free(ctx);
    return {
        Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) },
        Array<uint8_t>{},
        Array<uint8_t>{},
        Array<uint8_t>{}
    };
}

std::tuple<Error, Array<uint8_t>> UtilsAES::decrypt(const Array<uint8_t>& key, const Array<uint8_t>& input, const Array<uint8_t>& aad, const Array<uint8_t>& iv, const Array<uint8_t>& tag) {
    if (iv.length() != LENGTH_IV || tag.length() != LENGTH_AUTHEN_TAG) {
        return {
            Error{ GET_FUNC_NAME(), "Length of iv or tag is not default value in 'message/define.h'" },
            Array<uint8_t>{}
        };
    }

    if (key.length() != LENGTH_KEY) {
        return {
            Error{ GET_FUNC_NAME(), "Length of key must be 32" },
            Array<uint8_t>{}
        };
    }

    Array<uint8_t> output;
    EVP_CIPHER_CTX* ctx;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        goto handleError;
    }

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)) {
        goto handleError;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, LENGTH_IV, nullptr)) {
        goto handleError;
    }

    if (!EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.get(), iv.get())) {
        goto handleError;
    }

    int32_t outLen;
    if (!EVP_DecryptUpdate(ctx, nullptr, &outLen, aad.get(), aad.length())) {
        goto handleError;
    }

    output.reset(input.length());
    if (!EVP_DecryptUpdate(ctx, output.get(), &outLen, input.get(), input.length())) {
        goto handleError;
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, LENGTH_AUTHEN_TAG, (void*)tag.get())) {
        goto handleError;
    }

    if (!EVP_DecryptFinal_ex(ctx, output.get() + outLen, &outLen)) {
        goto handleError;
    }

    EVP_CIPHER_CTX_free(ctx);
    return { Error{}, std::move(output) };

handleError:
    EVP_CIPHER_CTX_free(ctx);
    return {
        Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) },
        Array<uint8_t>{}
    };
}