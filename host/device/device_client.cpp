#include "device_client.h"

#include <QElapsedTimer>
#include <QTcpSocket>

#include <algorithm>

extern "C" {
#include "protocol.h"
}

namespace {

constexpr qint64 replyTimeoutMs = 1000;
constexpr qint64 cancellationPollMs = 50;

std::optional<ExchangeResult> decodeReply(
    const ProtocolFrame &reply, std::uint16_t requestId,
    std::uint8_t expectedType, unsigned expectedLength
)
{
    if (reply.request_id != requestId) {
        return std::nullopt;
    }

    if (reply.type == 0xFFu && reply.payload_length == 1u) {
        return ExchangeResult{ExchangeStatus::CommandError, {}, reply.payload[0]};
    }

    if (reply.type != expectedType || reply.payload_length != expectedLength) {
        return std::nullopt;
    }

    if (reply.type == 0x83u) {
        const auto *p = reply.payload;
        const auto pressure = std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
                              (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        return ExchangeResult{ExchangeStatus::Ok, pressure, {}};
    }

    return ExchangeResult{ExchangeStatus::Ok, {}, {}};
}

} // namespace

DeviceClient::DeviceClient(
    QTcpSocket &socket,
    const std::atomic_bool *cancelRequested
)
    : socket_(socket), cancelRequested_(cancelRequested)
{
}

ExchangeResult DeviceClient::setMode(std::uint8_t mode)
{
    return exchange({0x01u, &mode, 1u, 0x81u, 0u});
}

ExchangeResult DeviceClient::setStimulus(std::uint32_t pressurePa)
{
    const std::uint8_t payload[] = {
        static_cast<std::uint8_t>(pressurePa),
        static_cast<std::uint8_t>(pressurePa >> 8),
        static_cast<std::uint8_t>(pressurePa >> 16),
        static_cast<std::uint8_t>(pressurePa >> 24)
    };

    return exchange({0x02u, payload, sizeof payload, 0x82u, 0u});
}

ExchangeResult DeviceClient::readSample()
{
    return exchange({0x03u, nullptr, 0u, 0x83u, 4u});
}

bool DeviceClient::isCancellationRequested() const
{
    return cancelRequested_ && cancelRequested_->load();
}

bool DeviceClient::sendRequest(const Request &request, std::uint16_t id)
{
    std::uint8_t bytes[PROTOCOL_MAX_FRAME_SIZE];
    const auto length = static_cast<qint64>(protocol_encode_frame(
        bytes, sizeof bytes, request.type, id, request.payload, request.payloadLength
    ));

    return length != 0 &&
           socket_.write(reinterpret_cast<const char *>(bytes), length) == length;
}

ExchangeResult DeviceClient::exchange(const Request &request)
{
    if (isCancellationRequested()) {
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
    QElapsedTimer deadline;
    deadline.start();

    if (!sendRequest(request, id)) {
        return {ExchangeStatus::ConnectionError, {}, {}};
    }

    return waitForReply(request, id, deadline);
}

ExchangeResult DeviceClient::waitForReply(
    const Request &request, std::uint16_t id, const QElapsedTimer &deadline
)
{
    ProtocolDecoder decoder;
    ProtocolFrame response;
    protocol_decoder_init(&decoder);

    while (deadline.elapsed() < replyTimeoutMs) {
        if (isCancellationRequested()) {
            return {ExchangeStatus::Cancelled, {}, {}};
        }

        if (socket_.bytesAvailable() == 0) {
            if (socket_.state() != QAbstractSocket::ConnectedState) {
                return {ExchangeStatus::ConnectionError, {}, {}};
            }

            const auto remainingMs = replyTimeoutMs - deadline.elapsed();
            if (remainingMs <= 0) {
                break;
            }

            const int waitMs = static_cast<int>(std::min(remainingMs, cancellationPollMs));

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
            if (nowMs >= replyTimeoutMs) {
                return {ExchangeStatus::Timeout, {}, {}};
            }

            protocol_decoder_push(&decoder, byte, nowMs);

            while (protocol_decoder_take(&decoder, nowMs, &response)) {
                const auto result =
                    decodeReply(response, id, request.expectedType, request.expectedLength);
                if (result) {
                    return *result;
                }
            }
        }
    }

    if (socket_.state() != QAbstractSocket::ConnectedState) {
        return {ExchangeStatus::ConnectionError, {}, {}};
    }
    return {ExchangeStatus::Timeout, {}, {}};
}