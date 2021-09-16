#ifndef CSO_CONNECTOR_INTERFACE_H
#define CSO_CONNECTOR_INTERFACE_H

#include <string>
#include "error/error.h"
#include "entity/array.hpp"

class IConnector {
public:
    virtual void listen(Error(*cb)(const std::string& sender, const Array<uint8_t>& data)) = 0;

    virtual Error sendMessage(const std::string& recvName, const Array<uint8_t>& content, bool isEncrypted, bool isCache) = 0;
    virtual Error sendGroupMessage(const std::string& groupName, const Array<uint8_t>& content, bool isEncrypted, bool isCache) = 0;

    virtual Error sendMessageAndRetry(const std::string& recvName, const Array<uint8_t>& content, bool isEncrypted, int32_t retry) = 0;
    virtual Error sendGroupMessageAndRetry(const std::string& groupName, const Array<uint8_t>& content, bool isEncrypted, int32_t retry) = 0;
};

#endif // !CSO_CONNECTOR_INTERFACE_H