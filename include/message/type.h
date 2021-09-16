#ifndef CSO_MESSAGE_TYPE_H
#define CSO_MESSAGE_TYPE_H

#include <cstdint>

enum MessageType : uint8_t {
    Activation   = 2U,
    Single       = 3U,
    Group        = 4U,
    SingleCached = 5U,
    GroupCached  = 6U,
    Done         = 7U
};

#endif // !CSO_MESSAGE_TYPE_H