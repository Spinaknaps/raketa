#pragma once

#include <cstdint>
#include "exchange_result.h"
#include <atomic>

class QTcpSocket;
class QElapsedTimer;

class DeviceClient {
public:
    explicit DeviceClient(
        QTcpSocket &socket,
        const std::atomic_bool *cancelRequested = nullptr
    );

    ExchangeResult setMode(std::uint8_t mode);
    ExchangeResult setStimulus(std::uint32_t pressurePa);
    ExchangeResult readSample();

private:
    struct Request {
        std::uint8_t type;
        const std::uint8_t *payload;
        unsigned payloadLength;
        std::uint8_t expectedType;
        unsigned expectedLength;
    };

    bool isCancellationRequested() const;
    bool sendRequest(const Request &request, std::uint16_t id);
    ExchangeResult exchange(const Request &request);
    ExchangeResult waitForReply(const Request &request, std::uint16_t id,
                                const QElapsedTimer &deadline);

    QTcpSocket &socket_;
    std::uint32_t nextId_ = 1u;
    const std::atomic_bool *cancelRequested_;
};