#if defined(__clang__) || defined(__GNUC__) // clang and GCC
#include <cstring>
#elif defined(_MSC_VER) // MSVC
#define CURL_STATICLIB
#include <Windows.h>
#endif

#include <curl/curl.h>
#include <openssl/err.h>
#include "error/thirdparty.h"
#include "utils/utils_general.hpp"

std::string ThirdParty::getOpensslError(int32_t code) {
    char reason[300];
    ERR_error_string_n(code, reason, 300);
    return format("[OpenSSL]: %s", reason);
}

std::string ThirdParty::getCurlError(int32_t code) {
    // "curl_easy_strerror" reutrns static array
    const char* reason = curl_easy_strerror((CURLcode)code);
    return format("[CURL]: %s", reason);
}

std::string ThirdParty::getSocketError(int32_t code) {
#if defined(__clang__) || defined(__GNUC__) // clang and GCC
    char reason[256];
    strerror_r(reasonCode, reason, sizeof(reason));
    return  format("[Socket]: %s", reason);
#elif defined(_MSC_VER) // MSVC
    char* reason = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&reason,
        0,
        nullptr
    );
    auto result = format("[Socket]: %s", reason);
    LocalFree(reason);
    return result;
#endif
}