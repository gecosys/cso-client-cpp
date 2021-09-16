#ifndef CSO_MESSAGE_READY_TICKET_H
#define CSO_MESSAGE_READY_TICKET_H

#include <tuple>
#include <memory>
#include <cstdint>
#include "error/error.h"
#include "entity/array.hpp"

class ReadyTicket {
private:
    bool isReady;
    uint32_t maskRead;
    uint64_t idxRead;
    uint64_t idxWrite;

public:
    ReadyTicket() noexcept;
    ~ReadyTicket() noexcept;

    bool getIsReady() noexcept;
    uint64_t getIdxRead() noexcept;
    uint32_t getMaskRead() noexcept;
    uint64_t getIdxWrite() noexcept;

    static std::tuple<Error, std::unique_ptr<ReadyTicket>> parseBytes(const Array<uint8_t>& data) noexcept;
};

#endif // !CSO_MESSAGE_READY_TICKET_H