#include "device_client.h"

#include <QElapsedTimer>
#include <QTcpSocket>

extern "C" {
#include "protocol.h"
}

DeviceClient::DeviceClient(
    QTcpSocket &socket,
    const std::atomic_bool *cancelRequested
)
    : socket_(socket), cancelRequested_(cancelRequested)
{
}

ExchangeResult DeviceClient::setMode(std::uint8_t mode)
{
    return exchange(0x01u, &mode, 1u, 0x81u, 0u);
}

ExchangeResult DeviceClient::setStimulus(std::uint32_t pressurePa)
{
    const std::uint8_t payload[] = {
        static_cast<std::uint8_t>(pressurePa),
        static_cast<std::uint8_t>(pressurePa >> 8),
        static_cast<std::uint8_t>(pressurePa >> 16),
        static_cast<std::uint8_t>(pressurePa >> 24)
    };

    return exchange(0x02u, payload, sizeof payload, 0x82u, 0u);
}

ExchangeResult DeviceClient::readSample()
{
    return exchange(0x03u, nullptr, 0u, 0x83u, 4u);
}

ExchangeResult DeviceClient::exchange(
    std::uint8_t type,
    const std::uint8_t *payload,
    unsigned payloadLength,
    std::uint8_t expectedType,
    unsigned expectedLength
)
{
    if (cancelRequested_ && cancelRequested_->load()) {
        return {ExchangeStatus::Cancelled, {}, {}};
    }

    if (socket_.state() != QAbstractSocket::ConnectedState) {
        return {ExchangeStatus::ConnectionError, {}, {}};
    }

    if (nextId_ > 0xFFFFu) {
        socket_.abort();
        return {ExchangeStatus::ConnectionError, {}, {}};
    }
    const auto id = static_cast<std::uint16_t>(nextId_++);

    std::uint8_t bytes[PROTOCOL_MAX_FRAME_SIZE];
    const auto length = static_cast<qint64>(protocol_encode_frame(
        bytes, sizeof bytes, type, id, payload, payloadLength
    ));

    QElapsedTimer deadline;
    deadline.start();

    if (length == 0 ||
        socket_.write(reinterpret_cast<const char *>(bytes), length) != length) {
        return {ExchangeStatus::ConnectionError, {}, {}};
    }

    ProtocolDecoder decoder;
    ProtocolFrame response;
    protocol_decoder_init(&decoder);

    while (deadline.elapsed() < 1000) {
        if (cancelRequested_ && cancelRequested_->load()) {
            return {ExchangeStatus::Cancelled, {}, {}};
        }

        if (socket_.bytesAvailable() == 0) {
            if (socket_.state() != QAbstractSocket::ConnectedState) {
                return {ExchangeStatus::ConnectionError, {}, {}};
            }

            const auto remainingMs = 1000 - deadline.elapsed();
            if (remainingMs <= 0) {
                break;
            }

            const int waitMs = remainingMs > 50 ? 50 : static_cast<int>(remainingMs);

            if (!socket_.waitForReadyRead(waitMs)) {
                if (socket_.state() != QAbstractSocket::ConnectedState ||
                    socket_.error() != QAbstractSocket::SocketTimeoutError) {
                    return {ExchangeStatus::ConnectionError, {}, {}};
                    }
                continue;
            }
        }

        for (unsigned char byte : socket_.readAll()) {
            const auto nowMs = static_cast<std::uint32_t>(deadline.elapsed());
            if (nowMs >= 1000u) {
                return {ExchangeStatus::Timeout, {}, {}};
            }

            protocol_decoder_push(&decoder, byte, nowMs);

            while (protocol_decoder_take(&decoder, nowMs, &response)) {
                if (response.request_id != id) {
                    continue;
                }

                if (response.type == 0xFFu && response.payload_length == 1u) {
                    return {ExchangeStatus::CommandError, {}, response.payload[0]};
                }

                if (response.type != expectedType ||
                    response.payload_length != expectedLength) {
                    continue;
                }

                if (expectedType == 0x83u) {
                    const auto *p = response.payload;
                    const auto pressurePa =
                        std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
                        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
                    return {ExchangeStatus::Ok, pressurePa, {}};
                }

                return {ExchangeStatus::Ok, {}, {}};
            }
        }
    }

    if (socket_.state() != QAbstractSocket::ConnectedState) {
        return {ExchangeStatus::ConnectionError, {}, {}};
    }
    return {ExchangeStatus::Timeout, {}, {}};
}