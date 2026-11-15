#pragma once

#include <cstdint>
#include <optional>
#include <atomic>

class QTcpSocket;

enum class ExchangeStatus {
    Ok,
    Timeout,
    CommandError,
    ConnectionError,
    Cancelled
};

struct ExchangeResult {
    ExchangeStatus status;
    std::optional<std::uint32_t> pressurePa;
    std::optional<std::uint8_t> errorCode;
};

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
    ExchangeResult exchange(
        std::uint8_t type,
        const std::uint8_t *payload,
        unsigned payloadLength,
        std::uint8_t expectedType,
        unsigned expectedLength
    );

    QTcpSocket &socket_;
    std::uint32_t nextId_ = 1u;
    const std::atomic_bool *cancelRequested_;
};