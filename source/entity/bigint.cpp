#include <openssl/err.h>
#include "entity/bigint.h"
#include "error/thirdparty.h"
#include "utils/utils_edian.h"
#include "utils/utils_define.h"

BigInt::BigInt() noexcept
    : bn{ BN_new() } {}

BigInt::BigInt(int64_t n) {
    // BN of OpenSSL uses big-edian
    int64_t m{ n };
    if (!UtilsEdian::isBigEndian()) {
        m = UtilsEdian::swap8Bytes(n);
    }
    this->bn = BN_bin2bn((const uint8_t*)&m, sizeof(int64_t), nullptr);
}

BigInt::BigInt(const BigInt& other)
    : bn{ BN_dup(other.bn) } {}

BigInt::BigInt(BigInt&& other) noexcept
    : bn{ BN_new() } {
    std::swap(this->bn, other.bn);
}

BigInt::~BigInt() noexcept {
    BN_free(this->bn);
}

BigInt& BigInt::operator=(BigInt&& other) noexcept {
    BN_free(this->bn);
    this->bn = BN_new();
    std::swap(this->bn, other.bn);
    return *this;
}

BigInt& BigInt::operator=(const BigInt& other) {
    BN_free(this->bn);
    this->bn = BN_dup(other.bn);
    return *this;
}

Error BigInt::setString(const std::string& str, int8_t radix) {
    BIGNUM* tmp{ nullptr };

    if (radix == 10) {
        tmp = BN_new();
        if (!BN_dec2bn(&tmp, str.c_str())) {
            goto handleError;
        }
    }
    if (radix == 16) {
        tmp = BN_new();
        if (!BN_hex2bn(&tmp, str.c_str())) {
            goto handleError;
        }
    }

    if (tmp != nullptr) {
        BN_free(this->bn);
        this->bn = tmp;
        return Error{};
    }
    return Error{ GET_FUNC_NAME(), "Radix must be 10 or 16" };

handleError:
    return Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) };
}

std::tuple<Error, std::string> BigInt::toString(int8_t radix) const {
    char* str{ nullptr };

    if (radix == 10) {
        str = BN_bn2dec(this->bn);
    }
    if (radix == 16) {
        str = BN_bn2hex(this->bn);
    }

    if (str != nullptr) {
        std::string output{ str };
        OPENSSL_free(str);
        return { Error{}, output };
    }
    return {
        Error{ GET_FUNC_NAME(), "Radix must be 10 or 16" },
        ""
    };
}

std::tuple<Error, BigInt> BigInt::powMod(const BigInt& power, const BigInt& modulus) const {
    BigInt output{};
    BN_CTX* ctx{};

    ctx = BN_CTX_new();
    if (ctx == nullptr) {
        goto handleError;
    }

    if (!BN_mod_exp(output.bn, this->bn, power.bn, modulus.bn, ctx)) {
        goto handleError;
    }

    BN_CTX_free(ctx);
    return std::make_tuple(Error{}, std::move(output));

handleError:
    BN_CTX_free(ctx);
    return {
        Error{ GET_FUNC_NAME(), ThirdParty::getOpensslError(ERR_get_error()) },
        BigInt{}
    };
}