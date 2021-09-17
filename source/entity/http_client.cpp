#include "entity/http_client.h"
#include "utils/utils_general.hpp"
#include "error/thirdparty.h"

static size_t cbWrite(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

SpinLock HttpClient::lock;
uint8_t HttpClient::nObjects{ 0 };

HttpClient::HttpClient() noexcept
    : curl{ nullptr },
      headers{ nullptr } {}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : curl{ nullptr },
      headers{ nullptr } {
    std::swap(this->curl, other.curl);
    std::swap(this->headers, other.headers);
}

HttpClient::~HttpClient() {
    // "curl" just is created when calls method "get", "post", "put", "delete".
    // The methods will clean up when done.
    // So we just free "headers" if the user sets header and do nothing.
    if (this->headers != nullptr) {
        curl_slist_free_all(this->headers);
    }
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this->headers != nullptr) {
        curl_slist_free_all(this->headers);
        this->headers = nullptr;
    }
    // "curl" just is created when calls method "get", "post", "put", "delete".
    // The methods will clean up when done.
    // So we just move "headers" if the user sets header and do nothing.
    std::swap(this->headers, other.headers);
    return *this;
}

Error HttpClient::init() {
    CURLcode errcode;
    HttpClient::lock.lock();
    if (HttpClient::nObjects++ > 0) {
        goto handleSuccess;
    }

    errcode = curl_global_init(CURL_GLOBAL_ALL);
    if (errcode == CURLcode::CURLE_OK) {
        goto handleSuccess;
    }
    HttpClient::lock.unlock();

    return Error{ GET_FUNC_NAME(), ThirdParty::getCurlError(errcode) };

handleSuccess:
    HttpClient::lock.unlock();
    return Error{};
}

void HttpClient::cleanup() {
    // Clean unused memory
    curl_slist_free_all(this->headers);
    curl_easy_cleanup(this->curl);
    this->headers = nullptr;
    this->curl = nullptr;

    // Free global share memory if the last object
    HttpClient::lock.lock();
    if (--HttpClient::nObjects == 0) {
        curl_global_cleanup();
    }
    HttpClient::lock.unlock();
}

void HttpClient::setHeader(const std::string& key, const std::string& value) {
    this->headers = curl_slist_append(
        this->headers,
        format("%s:%s", key.c_str(), value.c_str()).c_str()
    );
}

std::tuple<Error, std::string> HttpClient::post(const std::string& url, const std::string& body, long timeout) {
    Error err = init();
    if (!err.nil()) {
        return { err, "" };
    }

    this->curl = curl_easy_init();
    if (this->curl == nullptr) {
        cleanup();
        return { Error{ GET_FUNC_NAME(), "Init curl object failed" } , "" };
    }

    curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
    if (this->headers != nullptr) {
        curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, this->headers);
    }
    curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, timeout);
    //Set to non-zero means the operation is POST
    curl_easy_setopt(this->curl, CURLOPT_POST, 1);
    //  Set the JSON data to be POSTed

    curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, body.c_str());
    //Set uploadjson string length, this setting can be ignored
    curl_easy_setopt(this->curl, CURLOPT_POSTFIELDSIZE, body.length());
    //Set callback function
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, cbWrite);
    //Set write data
    std::string resp{};
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &resp);
    auto errCode = curl_easy_perform(this->curl);
    cleanup();

    if (errCode == CURLcode::CURLE_OK) {
        return { Error{}, std::move(resp) };
    }
    return { Error{ GET_FUNC_NAME(), ThirdParty::getCurlError((int32_t)errCode) }, "" };
}