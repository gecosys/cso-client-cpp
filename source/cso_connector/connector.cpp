#include <thread>
#include <chrono>
#include <iostream>
#include "cso_config/config.h"
#include "cso_queue/queue.h"
#include "cso_queue/item.h"
#include "cso_proxy/proxy.h"
#include "cso_parser/parser.h"
#include "cso_counter/counter.h"
#include "cso_connection/connection.h"
#include "cso_connector/connector.h"
#include "message/ready_ticket.h"
#include "utils/utils_define.h"

#define Millis_Timestamp() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()

// inits a new instance of Connector interface with default values
std::unique_ptr<IConnector> Connector::build(int32_t bufferSize, std::unique_ptr<IConfig>&& config) {
    return std::unique_ptr<IConnector>(new Connector(
        bufferSize,
        Queue::build(bufferSize),
        Parser::build(),
        Proxy::build(std::forward<std::unique_ptr<IConfig>>(config))
    ));
}

// inits a new instance of Connector interface
std::unique_ptr<IConnector> Connector::build(int32_t bufferSize, std::unique_ptr<IQueue>&& queue, std::unique_ptr<IParser>&& parser, std::unique_ptr<IProxy>&& proxy) {
    return std::unique_ptr<IConnector>(new Connector(
        bufferSize,
        std::forward<std::unique_ptr<IQueue>>(queue),
        std::forward<std::unique_ptr<IParser>>(parser),
        std::forward<std::unique_ptr<IProxy>>(proxy)
    ));
}

Connector::Connector(
    int32_t bufferSize,
    std::unique_ptr<IQueue>&& queue,
    std::unique_ptr<IParser>&& parser,
    std::unique_ptr<IProxy>&& proxy
) : time{ 0 },
    isActivated{ false },
    isDisconnected{ false },
    proxy{ nullptr },
    parser{ nullptr },
    counter{ nullptr },
    connection{ Connection::build(bufferSize) },
    queueMessages{ nullptr } {
    this->proxy.swap(proxy);
    this->parser.swap(parser);
    this->queueMessages.swap(queue);
}

void Connector::listen(Error(*cb)(const std::string& sender, const Array<uint8_t>& data)) {
    // Keep connection to Cloud Socket system
    std::thread thread(&Connector::loopReconnect, this);
    thread.detach();

    while (true) {
        // Receive message response
        {
            Array<uint8_t> cipher = this->connection->getMessage();
            if (!cipher.empty()) {
                Error err;
                std::unique_ptr<Cipher> message;

                std::tie(err, message) = this->parser->parseReceivedMessage(cipher);
                if (!err.nil()) {
                    std::cerr << "[E]" << err.toString() << std::endl;
                    continue;
                }

                MessageType type = message->getMsgType();
                // Activate the connection
                if (type == MessageType::Activation) {
                    std::unique_ptr<ReadyTicket> readyTicket;
                    std::tie(err, readyTicket) = ReadyTicket::parseBytes(message->getData());
                    if (!err.nil() || !readyTicket->getIsReady()) {
                        continue;
                    }

                    this->isActivated.store(true, std::memory_order_release);
                    if (this->counter == nullptr) {
                        this->counter = Counter::build(readyTicket->getIdxWrite(), readyTicket->getIdxRead(), readyTicket->getMaskRead());
                    }
                    continue;
                }

                if (!this->isActivated.load(std::memory_order_acquire)) {
                    continue;
                }

                if (type != MessageType::Done &&
                    type != MessageType::Single &&
                    type != MessageType::SingleCached &&
                    type != MessageType::Group &&
                    type != MessageType::GroupCached) {
                    continue;
                }

                if (message->getMsgID() == 0) {
                    if (message->getIsRequest()) {
                        cb(message->getName(), message->getData());
                    }
                    continue;
                }

                if (!message->getIsRequest()) { //response
                    this->queueMessages->clearMessage(message->getMsgID());
                    continue;
                }

                if (this->counter->markReadDone(message->getMsgTag())) {
                    if (!cb(message->getName(), message->getData()).nil()) {
                        this->counter->markReadUnused(message->getMsgTag());
                        return;
                    }
                }

                Array<uint8_t> newMessage;
                std::tie(err, newMessage) = this->parser->buildMessage(
                    message->getMsgID(),
                    message->getMsgTag(),
                    message->getIsEncrypted(),
                    false,
                    true,
                    true,
                    false,
                    message->getName(),
                    Array<uint8_t>{}
                );
                if (!err.nil()) {
                    std::cerr << "[E]" << err.toString() << std::endl;
                    continue;
                }
                this->connection->sendMessage(newMessage);
            }
        }

        if (this->isDisconnected.load(std::memory_order_acquire)) {
            continue;
        }

        // Send message in queue
        if (this->isActivated.load(std::memory_order_acquire) && (Millis_Timestamp() - this->time) >= 100) {
            ItemQueueRef ref_item = this->queueMessages->nextMessage();
            if (ref_item.empty()) {
                this->time = Millis_Timestamp();
                continue;
            }

            Error err;
            Array<uint8_t> message;
            ItemQueue& item = ref_item.get();

            if (item.isGroup) {
                std::tie(err, message) = this->parser->buildGroupMessage(
                    item.msgID,
                    item.msgTag,
                    item.isEncrypted,
                    item.isCached,
                    item.isFirst,
                    item.isLast,
                    item.isRequest,
                    item.recvName,
                    item.content
                );
            }
            else {
                std::tie(err, message) = this->parser->buildMessage(
                    item.msgID,
                    item.msgTag,
                    item.isEncrypted,
                    item.isCached,
                    item.isFirst,
                    item.isLast,
                    item.isRequest,
                    item.recvName,
                    item.content
                );
            }

            if (!err.nil()) {
                this->time = Millis_Timestamp();
                std::cerr << "[E]" << err.toString() << std::endl;
                continue;
            }

            err = this->connection->sendMessage(message);
            if (!err.nil()) {
                std::cerr << "[E]" << err.toString() << std::endl;
            }
            this->time = Millis_Timestamp();
        }
    }
}

Error Connector::sendMessage(const std::string& recvName, const Array<uint8_t>& content, bool isEncrypted, bool isCache) {
    if (!this->isActivated.load(std::memory_order_acquire)) {
        return Error{ GET_FUNC_NAME(), "Connection is not activated yet" };
    }
    return doSendMessageNotRetry(recvName, content, false, isEncrypted, isCache);
}

Error Connector::sendGroupMessage(const std::string& groupName, const Array<uint8_t>& content, bool isEncrypted, bool isCache) {
    if (!this->isActivated.load(std::memory_order_acquire)) {
        return Error{ GET_FUNC_NAME(), "Connection is not activated yet" };
    }
    return doSendMessageNotRetry(groupName, content, true, isEncrypted, isCache);
}

Error Connector::sendMessageAndRetry(const std::string& recvName, const Array<uint8_t>& content, bool isEncrypted, int32_t retry) {
    if (!this->isActivated.load(std::memory_order_acquire)) {
        return Error{ GET_FUNC_NAME(), "Connection is not activated yet" };
    }

    if (!this->queueMessages->takeIndex()) {
        return Error{ GET_FUNC_NAME(), "Message queue is full" };
    }

    return doSendMessageRetry(recvName, content, false, isEncrypted, retry);
}

Error Connector::sendGroupMessageAndRetry(const std::string& groupName, const Array<uint8_t>& content, bool isEncrypted, int32_t retry) {
    if (!this->isActivated.load(std::memory_order_acquire)) {
        return Error{ GET_FUNC_NAME(), "Connection is not activated yet" };
    }

    if (!this->queueMessages->takeIndex()) {
        return Error{ GET_FUNC_NAME(), "Message queue is full" };
    }
    return doSendMessageRetry(groupName, content, true, isEncrypted, retry);
}

//========
// PRIVATE
//========

[[noreturn]] void Connector::loopReconnect() {
    Error err;
    ServerTicket ticket;

    while (true) {
        // Exchange key
        std::tie(err, ticket) = prepare();
        if (!err.nil()) {
            std::cerr << "[E]" << err.toString() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        // Connect to hub
        err = this->connection->doConnect(ticket.hubIP, ticket.hubPort);
        if (!err.nil()) {
            std::cerr << "[E]" << err.toString() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        // Set values
        this->parser->setSecretKey(std::move(ticket.serverSecretKey));
        this->isActivated.store(false, std::memory_order_release);
        this->isDisconnected.store(false, std::memory_order_release);

        //// Activate connection
        std::thread thread(&Connector::activateConnection, this, ticket.ticketID, ticket.ticketBytes);
        thread.detach();

        // Loop to receive message
        err = this->connection->loopListen();
        if (!err.nil()) {
            std::cerr << "[E]" << err.toString() << std::endl;
        }
        this->isDisconnected.store(true, std::memory_order_release);
    }
}

std::tuple<Error, ServerTicket> Connector::prepare() {
    Error err;
    ServerKey serverKey;

    std::tie(err, serverKey) = this->proxy->exchangeKey();
    if (!err.nil()) {
        return { std::move(err), ServerTicket{} };
    }
    return  this->proxy->registerConnection(serverKey);
}

void Connector::activateConnection(uint16_t ticketID, const Array<uint8_t>& ticketBytes) {
    Error err;
    Array<uint8_t> message;

    while (true) {
        if (this->isDisconnected.load(std::memory_order_acquire) ||
            this->isActivated.load(std::memory_order_acquire)) {
            break;
        }

        std::tie(err, message) = this->parser->buildActiveMessage(ticketID, ticketBytes);
        if (!err.nil()) {
            std::cerr << "[E]" << err.toString() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        err = this->connection->sendMessage(message);
        if (!err.nil()) {
            std::cerr << "[E]" << err.toString() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

Error Connector::doSendMessageNotRetry(const std::string& name, const Array<uint8_t>& content, bool isGroup, bool isEncrypted, bool isCache) {
    Error err;
    Array<uint8_t> message;

    if (!isGroup) {
        std::tie(err, message) = this->parser->buildMessage(0, 0, isEncrypted, isCache, true, true, true, name, content);
    }
    else {
        std::tie(err, message) = this->parser->buildGroupMessage(0, 0, isEncrypted, isCache, true, true, true, name, content);
    }
    if (!err.nil()) {
        return err;
    }
    return this->connection->sendMessage(message);
}

Error Connector::doSendMessageRetry(const std::string& recvName, const Array<uint8_t>& content, bool isGroup, bool isEncrypted, int32_t retry) {
    std::unique_ptr<ItemQueue> item{ new ItemQueue{} };
    item->content = content;
    item->msgID = this->counter->nextWriteIndex();
    item->msgTag = 0;
    item->recvName = recvName;
    item->isEncrypted = isEncrypted;
    item->isCached = false;
    item->isFirst = true;
    item->isLast = true;
    item->isRequest = true;
    item->isGroup = isGroup;
    item->numberRetry = retry + 1;
    item->timestamp = 0;
    this->queueMessages->pushMessage(std::move(item));
    return Error{};
}