#include <cstring>
#include "cso_connection/connection.h"
#include "utils/utils_define.h"
#include "utils/utils_edian.h"
#include "error/thirdparty.h"

#if defined(__clang__) || defined(__GNUC__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#elif defined(_MSC_VER)
#include <ws2tcpip.h>
#endif

#define HEADER_SIZE 2
#define BUFFER_SIZE 1024

std::unique_ptr<IConnection> Connection::build(size_t queueSize) {
    return std::unique_ptr<IConnection>{ new Connection(queueSize) };
}

Connection::Connection(size_t queueSize)
    : sock{},
      status{ Status::Disconnected },
      nextMsg{ queueSize } {
#if defined(__clang__) || defined(__GNUC__)
    this->sock = -1;
#elif defined(_MSC_VER)
    this->sock = INVALID_SOCKET;
#endif
}

Connection::~Connection() noexcept {
    cleanup();
}

Error Connection::doConnect(const std::string& host, uint16_t port) {
#if defined(__clang__) || defined(__GNUC__)
    struct sockaddr_in serverAddr;
    int32_t errCode;
    std::string errContent;

    this->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (this->sock < 0) {
        errContent = "Initialization socket failed";
        goto handleError;
    }

    memset(&serverAddr, '0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    errCode = inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);
    if (errCode == 0) {
        errContent = "Getting address failed";
        goto handleError;
    }
    if (errCode < 0) {
        errCode = errno;
        goto handleError;
    }

    errCode = connect(this->sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (errCode < 0) {
        errCode = errno;
        goto handleError;
    }

    this->status.store(Status::Connected, std::memory_order_release);
    return Error{};

handleError:
    if (!errContent.empty()) {
        return Error{ GET_FUNC_NAME(), std::move(errContent) };
    }
    return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(errCode) };

#elif defined(_MSC_VER) // MSVC
    struct addrinfo* result{ nullptr };
    struct addrinfo* ptr{ nullptr };
    struct addrinfo hints;
    WSADATA wsaData;
    int32_t errCode;
    std::string errContent;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return Error{ GET_FUNC_NAME(), "Initialization socket failed" };
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
        errContent = "Getting address failed";
        this->status.store(Status::Disconnected, std::memory_order_release);
        goto handleError;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        // Create a SOCKET for connecting to server
        this->sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (this->sock == INVALID_SOCKET) {
            errCode = WSAGetLastError();
            this->status.store(Status::Disconnected, std::memory_order_release);
            goto handleError;
        }

        // Connect to server.
        errCode = connect(this->sock, ptr->ai_addr, (int32_t)ptr->ai_addrlen);
        if (errCode == SOCKET_ERROR) {
            closesocket(this->sock);
            this->sock = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(result);

    if (this->sock == INVALID_SOCKET) {
        errContent = "Socket connection failed";
        this->status.store(Status::Disconnected, std::memory_order_release);
        goto handleError;
    }

    this->status.store(Status::Connected, std::memory_order_release);
    return Error{};

handleError:
    WSACleanup();
    if (!errContent.empty()) {
        return Error{ GET_FUNC_NAME(), std::move(errContent) };
    }
    return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(errCode) };
#endif
}

Error Connection::loopListen() {
    bool header{ true };
    uint8_t* message{ nullptr };
    uint16_t seek{ 0 };
    uint16_t readed{ 0 };
    uint16_t length{ HEADER_SIZE };
    int32_t errCode{ 0 };
    Array<uint8_t> buffer{ BUFFER_SIZE };

    while (this->status.load(std::memory_order_acquire) == Status::Connected) {
#if defined(__clang__) || defined(__GNUC__)
        readed = read(this->sock, buffer.get() + seek, length - seek);
        if (readed < 0) {
            errCode = errno;
            break;
        }
#elif defined(_MSC_VER)
        // "readed" always is <= "length - seek"
        // "recv" will block when data is comming
        readed = recv(this->sock, (char*)buffer.get() + seek, length - seek, 0);
        // Connection close
        if (readed == 0) {
            break;
        }
        // Error
        if (readed < 0) {
            errCode = WSAGetLastError();
            // message is larger than the buffer specified, 
            // the buffer is filled with the first part of the message
            // "recv" will generate the error WSAEMSGSIZE
            // see more at: https://docs.microsoft.com/vi-vn/windows/win32/api/winsock/nf-winsock-recv?redirectedfrom=MSDN
            if (errCode == WSAEMSGSIZE) {
                continue;
            }
            break;
        }
#endif

        // Read enough data
        seek += readed;
        if (seek < length) {
            continue;
        }

        // Read "data length"
        if (header) {
            length = *((uint16_t*)buffer.get());
            // Server returned the length is little edian
            // so we convert it if system is big edian
            if (UtilsEdian::isBigEndian()) {
                length = UtilsEdian::swap2Bytes(length);
            }
            if (length > 0) {
                header = false;
            }
            seek = 0;
            continue;
        }

        // Push message
        // Don't delete "message" because queue will manage memory
        message = new uint8_t[length];
        memcpy(message, buffer.get(), length);
        this->nextMsg.push(Array<uint8_t>{message, length});

        // Reset
        length = HEADER_SIZE;
        header = true;
        seek = 0;
    }

    // cleanup
    auto err = cleanup();
    if (!err.nil()) {
        return err;
    }

    // If "errCode" != 0, "recv" occurred some error
    if (errCode != 0) {
        return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(errCode) };
    }
    return Error{ GET_FUNC_NAME(), "Socket is disconnected" };
}

Error Connection::sendMessage(const Array<uint8_t>& message) {
    if (this->status.load(std::memory_order_acquire) != Status::Connected) {
        return Error{ GET_FUNC_NAME(), "Socket connection is disconnected" };
    }

    // Build package
    Array<uint8_t> package;
    {
        auto lenMessage = message.length();
        package.reset(HEADER_SIZE + lenMessage);
        memcpy(package.get() + HEADER_SIZE, message.get(), lenMessage);
        if (UtilsEdian::isBigEndian()) {
            lenMessage = UtilsEdian::swap2Bytes((int16_t)lenMessage);
        }
        memcpy(package.get(), &lenMessage, HEADER_SIZE);
    }

    for (size_t offset = 0, sent = 0, lenPkg = package.length(); offset < lenPkg; offset += sent) {
#if defined(__clang__) || defined(__GNUC__)
        sent = send(this->sock, package.get() + offset, lenPkg - offset, 0);
#elif defined(_MSC_VER)
        sent = send(this->sock, (char*)package.get() + offset, lenPkg - offset, 0);
#endif
        if (sent < 0) {
            auto err = cleanup();
            if (!err.nil()) {
                return err;
            }

#if defined(__clang__) || defined(__GNUC__)
            return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(errno) };
#elif defined(_MSC_VER)
            return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(WSAGetLastError()) };
#endif
        }
    }
    return Error{};
}

Array<uint8_t> Connection::getMessage() noexcept {
    bool ok;
    Array<uint8_t> msg;

    std::tie(ok, msg) = this->nextMsg.try_pop();
    return ok ? msg : Array<uint8_t>{};
}

Error Connection::cleanup() {
#if defined(__clang__) || defined(__GNUC__)
    if (this->sock == -1) {
        goto handleSuccess;
    }

    if (shutdown(this->sock, SHUT_WR) < 0) {
        goto handleError;
    }

    if (close(this->sock) < 0) {
        goto handleError;
    }

handleSuccess:
    this->status.store(Status::Disconnected, std::memory_order_release);
    return Error{};

handleError:
    return Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(errno) };

#elif defined(_MSC_VER)
    if (this->sock == INVALID_SOCKET) {
        this->status.store(Status::Disconnected, std::memory_order_release);
        return Error{};
    }

    Error err{};
    if (shutdown(this->sock, SD_SEND) == SOCKET_ERROR) {
        err = Error{ GET_FUNC_NAME(), ThirdParty::getSocketError(WSAGetLastError()) };
    }

    closesocket(this->sock);
    WSACleanup();
    this->status.store(Status::Disconnected, std::memory_order_release);
    return err;
#endif
}