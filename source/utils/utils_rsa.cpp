#include <openssl/pem.h>
#include <openssl/err.h>
#include "utils/utils_rsa.h"
#include "utils/utils_define.h"
#include "encryption/hasing.h"
#include "error/thirdparty.h"

std::tuple<Error, EVP_PKEY*> UtilsRSA::parsePublicKey(const std::string& publicKey) {
    BIO* bio{};
    EVP_PKEY* pubKey{};

    bio = BIO_new_mem_buf(publicKey.c_str(), -1);
    if (bio == nullptr) {
        goto handleError;
    }

    pubKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (pubKey == nullptr) {
        goto handleError;
    }
    return { Error{}, pubKey };

handleError:
    return { Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) }, nullptr };
}

std::tuple<Error, bool> UtilsRSA::verifySignature(const std::string& publicKey, const Array<uint8_t>& signature, const Array<uint8_t>& data) {
    Error err{};
    EVP_PKEY* pubKey{};
    Array<uint8_t> hashed{};

    std::tie(err, hashed) = Hashing::sha256(data.get(), data.length());
    if (!err.nil()) {
        return { err, false };
    }

    std::tie(err, pubKey) = UtilsRSA::parsePublicKey(publicKey);
    if (!err.nil()) {
        return { err, false };
    }

    if (RSA_verify(NID_sha256, hashed.get(), hashed.length(), signature.get(), signature.length(), EVP_PKEY_get1_RSA(pubKey)) == 1) {
        EVP_PKEY_free(pubKey);
        return { Error{}, true };
    }

    EVP_PKEY_free(pubKey);
    return { Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) }, false };
}