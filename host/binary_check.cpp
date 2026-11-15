#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTextStream>
#include <QByteArray>

extern "C" {
#include "../firmware/protocol.h"
}

static bool exchange(
    QTcpSocket &socket, uint8_t type, uint16_t id,
    const uint8_t *payload, size_t payloadLength,
    uint8_t expectedType, size_t expectedLength, ProtocolFrame &response, bool sendBadCrcFirst = false,
    bool expectSilence = false
) {
    uint8_t bytes[PROTOCOL_MAX_FRAME_SIZE];
    const size_t length = protocol_encode_frame(
        bytes, sizeof bytes, type, id, payload, payloadLength
    );
    QTextStream err(stderr);

    if (length == 0u) {
        err << "Could not encode request ID " << id << Qt::endl;
        return false;
    }

    if (sendBadCrcFirst) {
        uint8_t badBytes[PROTOCOL_MAX_FRAME_SIZE];
        const size_t badLength = protocol_encode_frame(
            badBytes, sizeof badBytes, type, 0xFFFFu, payload, payloadLength
        );

        if (badLength == 0u) {
            err << "Could not encode bad CRC frame" << Qt::endl;
            return false;
        }

        // Меняем один бит в последнем байте CRC уже после кодирования.
        badBytes[badLength - 1u] ^= 0x01u;

        if (socket.write(reinterpret_cast<const char *>(badBytes),
                         static_cast<qint64>(badLength)) !=
            static_cast<qint64>(badLength)) {
            err << "Could not queue bad CRC frame" << Qt::endl;
            return false;
            }
    }

    QElapsedTimer deadline;
    deadline.start();

    if (socket.write(reinterpret_cast<const char *>(bytes), length) !=
        static_cast<qint64>(length)) {
        err << "Could not queue request ID " << id << Qt::endl;
        return false;
    }

    ProtocolDecoder decoder;
    protocol_decoder_init(&decoder);

    while (deadline.elapsed() < 1000) {
        if (socket.bytesAvailable() == 0) {
            const auto remainingMs = 1000 - deadline.elapsed();
            if (remainingMs <= 0 ||
                !socket.waitForReadyRead(static_cast<int>(remainingMs))) {
                break;
            }
        }

        for (unsigned char byte : socket.readAll()) {
            const auto nowMs = static_cast<uint32_t>(deadline.elapsed());
            protocol_decoder_push(&decoder, byte, nowMs);

            while (protocol_decoder_take(&decoder, nowMs, &response)) {
                if (sendBadCrcFirst && response.request_id == 0xFFFFu) {
                    err << "Emulator replied to a frame with bad CRC" << Qt::endl;
                    return false;
                }

                if (response.request_id != id) {
                    continue;
                }

                if (response.type == 0xFFu && response.payload_length == 1u) {
                    err << "Request ID " << id << " rejected with code "
                        << static_cast<unsigned>(response.payload[0]) << Qt::endl;
                    return false;
                }

                if (response.type == expectedType &&
                    response.payload_length == expectedLength) {
                    if (expectSilence) {
                        err << "Unexpected response in silent mode" << Qt::endl;
                        return false;
                    }
                    return true;
                    }
            }
        }
    }

    if (expectSilence &&
        socket.state() == QAbstractSocket::ConnectedState &&
        deadline.elapsed() >= 1000) {
        return true;
    }

    err << "No matching response for request ID " << id
            << " (" << socket.errorString() << ')' << Qt::endl;
    return false;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, 5555);
    if (!socket.waitForConnected(3000)) {
        err << "TCP connection failed: " << socket.errorString() << Qt::endl;
        return 1;
    }
    out << "Connected to 127.0.0.1:5555" << Qt::endl;

    ProtocolFrame response;
    uint16_t nextId = 1u;

    const auto setMode = [&](uint8_t mode) {
        return exchange(socket, 0x01u, nextId++, &mode, 1u, 0x81u, 0u, response);
    };

    const auto setPressure = [&](uint32_t pressure) {
        const uint8_t payload[] = {
            static_cast<uint8_t>(pressure),
            static_cast<uint8_t>(pressure >> 8),
            static_cast<uint8_t>(pressure >> 16),
            static_cast<uint8_t>(pressure >> 24)
        };
        return exchange(socket, 0x02u, nextId++, payload, sizeof payload,
                        0x82u, 0u, response);
    };

    const auto expectPressure = [&](uint32_t expected) {
        if (!exchange(socket, 0x03u, nextId++, nullptr, 0u, 0x83u, 4u, response)) {
            return false;
        }

        const auto *p = response.payload;
        const uint32_t actual = uint32_t(p[0]) | (uint32_t(p[1]) << 8)
                                | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        if (actual != expected) {
            err << "Expected " << expected << " Pa, got " << actual << " Pa"
                    << Qt::endl;
            return false;
        }
        return true;
    };

    if (!setMode(0u)) {
        return 1;
    }
    const uint32_t steps[] = {100000u, 90000u, 80000u};
    for (uint32_t step: steps) {
        if (!setPressure(step) || !expectPressure(step)) {
            return 1;
        }
    }
    out << "normal OK" << Qt::endl;

    if (!setMode(1u) ||
        !setPressure(100000u) || !expectPressure(100000u) ||
        !setPressure(90000u) || !expectPressure(100000u)) {
        return 1;
    }
    out << "stuck OK" << Qt::endl;

    if (!setMode(1u) || !setPressure(80000u) || !expectPressure(80000u)) {
        return 1;
    }
    out << "mode reset OK" << Qt::endl;

    if (!setMode(2u) || !setPressure(100000u) ||
        !exchange(socket, 0x03u, nextId++, nullptr, 0u, 0x83u, 4u,
                  response, false, true)) {
        return 1;
    }
    out << "silent OK" << Qt::endl;

    if (!setMode(0u) || !setPressure(100000u)) {
        return 1;
    }

    // Повреждённый SET_MODE предлагает переключить прибор в silent.
    // Корректный SET_MODE следом запрашивает тот же режим.
    const uint8_t silent = 2u;
    if (!exchange(socket, 0x01u, nextId++, &silent, 1u, 0x81u, 0u,
                  response, true)) {
        return 1;
    }
    out << "response after bad CRC OK" << Qt::endl;

    return 0;
}
