#ifndef CSO_ENTITY_CLIENT_H
#define CSO_ENTITY_CLIENT_H

#if defined(_MSC_VER) // MSVC
#define CURL_STATICLIB
#endif

#include <tuple>
#include <string>
#include <curl/curl.h>

#include "error/error.h"
#include "entity/spinlock.h"

class HttpClient {
private:
    CURL* curl;
    curl_slist* headers;

private:
    // "libcurl" uses global shared memory for each created object, so shared memory will be not thread-safe.
    // Otherwise, only the first thread is created global shared memory.
    // Therefore, we need to synchronize and manage the number of created objects.
    static SpinLock lock;
    static uint8_t nObjects;

    Error init();
    void cleanup();

public:
    HttpClient() noexcept;
    HttpClient(HttpClient&& other) noexcept;
    HttpClient(const HttpClient& other) = delete;
    ~HttpClient();

    HttpClient& operator=(HttpClient&& other) noexcept;
    HttpClient& operator=(const HttpClient& other) = delete;

    void setHeader(const std::string& key, const std::string& value);
    std::tuple<Error, std::string> post(const std::string& url, const std::string& body, long timeout = 5);
};

#endif // !CSO_ENTITY_CLIENT_H