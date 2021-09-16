#ifndef CSO_CONNECTION_H
#define CSO_CONNECTION_H

#if defined(__clang__) || defined(__GNUC__) // clang and GCC
#include <cstdint>
using Socket = int32_t;
#elif defined(_MSC_VER) // MSVC
#include <WinSock2.h>
using Socket = SOCKET;
#endif

#include <memory>
#include <atomic>
#include "status.h"
#include "interface.h"
#include "entity/spsc_queue.hpp"

class Connection : public IConnection {
private:
    Socket sock;
    std::atomic<uint8_t> status;
    SPSCQueue<Array<uint8_t>, true> nextMsg;

public:
    static std::unique_ptr<IConnection> build(size_t queueSize);

private:
    Connection(size_t queueSize);

    Error cleanup();

public:
    Connection() = delete;
    Connection(Connection&& other) = delete;
    Connection(const Connection& other) = delete;
    ~Connection() noexcept;


    Connection& operator=(Connection&& other) = delete;
    Connection& operator=(const Connection& other) = delete;

    Error doConnect(const std::string& host, uint16_t port);
    Error loopListen();
    Error sendMessage(const Array<uint8_t>& messsage);
    Array<uint8_t> getMessage() noexcept;
};

#endif // !CSO_CONNECTION_H