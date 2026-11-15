#include "device_client.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>

#include <future>
#include <thread>

extern "C" {
#include "protocol.h"
}

namespace {

constexpr int serverWaitMs = 4000;

bool acceptConnection(QTcpServer &server)
{
    return server.hasPendingConnections() ||
           server.waitForNewConnection(serverWaitMs);
}

bool receiveReadRequest(QTcpSocket &peer, std::uint16_t &id)
{
    ProtocolDecoder decoder;
    protocol_decoder_init(&decoder);

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < serverWaitMs) {
        if (peer.bytesAvailable() == 0) {
            const auto remaining = serverWaitMs - timer.elapsed();
            if (remaining <= 0 ||
                !peer.waitForReadyRead(static_cast<int>(remaining))) {
                return false;
            }
        }

        const QByteArray bytes = peer.readAll();
        for (unsigned char byte : bytes) {
            const auto now = static_cast<std::uint32_t>(timer.elapsed());
            protocol_decoder_push(&decoder, byte, now);

            ProtocolFrame frame;
            if (protocol_decoder_take(&decoder, now, &frame)) {
                if (frame.type != 0x03u || frame.payload_length != 0u) {
                    return false;
                }
                id = frame.request_id;
                return true;
            }
        }
    }

    return false;
}

QByteArray sampleReply(std::uint16_t id, std::uint32_t pressure)
{
    const std::uint8_t payload[] = {
        static_cast<std::uint8_t>(pressure),
        static_cast<std::uint8_t>(pressure >> 8),
        static_cast<std::uint8_t>(pressure >> 16),
        static_cast<std::uint8_t>(pressure >> 24)
    };

    std::uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
    const auto size = protocol_encode_frame(
        frame, sizeof frame, 0x83u, id, payload, sizeof payload
    );
    return QByteArray(
        reinterpret_cast<const char *>(frame), static_cast<qsizetype>(size)
    );
}

bool sendReplies(QTcpSocket &peer, const QByteArray &bytes)
{
    if (bytes.isEmpty() || peer.write(bytes) != bytes.size()) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    while (peer.bytesToWrite() > 0) {
        const auto remaining = serverWaitMs - timer.elapsed();
        if (remaining <= 0 ||
            !peer.waitForBytesWritten(static_cast<int>(remaining))) {
            return false;
        }
    }

    return true;
}

bool runServer(std::promise<quint16> &portReady)
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        portReady.set_value(0);
        return false;
    }
    portReady.set_value(server.serverPort());

    // Первое подключение. Прочитать запрос и закрыть соединение.
    if (!acceptConnection(server)) {
        return false;
    }
    auto *first = server.nextPendingConnection();
    std::uint16_t firstId;
    if (!receiveReadRequest(*first, firstId)) {
        return false;
    }
    first->disconnectFromHost();
    if (first->state() != QAbstractSocket::UnconnectedState &&
        !first->waitForDisconnected(serverWaitMs)) {
        return false;
    }

    // Второе подключение. Сначала подтвердить, что обмен восстановился.
    if (!acceptConnection(server)) {
        return false;
    }
    auto *second = server.nextPendingConnection();
    std::uint16_t reconnectId;
    if (!receiveReadRequest(*second, reconnectId) ||
        !sendReplies(*second, sampleReply(reconnectId, 100000u))) {
        return false;
    }

    // На запрос A пока не отвечать. Клиент пришлёт B после тайм-аута A.
    std::uint16_t oldId;
    std::uint16_t currentId;
    if (!receiveReadRequest(*second, oldId) ||
        !receiveReadRequest(*second, currentId) ||
        oldId == currentId) {
        return false;
    }

    // Отправить старый и актуальный ответы в указанном порядке.
    const QByteArray replies =
        sampleReply(oldId, 11111u) + sampleReply(currentId, 22222u);
    if (!sendReplies(*second, replies)) {
        return false;
    }

    // Оставить соединение открытым, пока клиент проверяет показание.
    return second->state() == QAbstractSocket::UnconnectedState ||
           second->waitForDisconnected(serverWaitMs);
}

bool runClient(quint16 port)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (port == 0) {
        err << "Could not start the test server" << Qt::endl;
        return false;
    }

    {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        if (!socket.waitForConnected(2000)) {
            err << "First connection failed" << Qt::endl;
            return false;
        }

        DeviceClient client(socket);
        if (client.readSample().status != ExchangeStatus::ConnectionError) {
            err << "Expected ConnectionError during read" << Qt::endl;
            return false;
        }
        out << "Disconnect during read OK" << Qt::endl;
    }

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(2000)) {
        err << "Reconnect failed" << Qt::endl;
        return false;
    }

    DeviceClient client(socket);
    const auto reconnected = client.readSample();
    if (reconnected.status != ExchangeStatus::Ok ||
        reconnected.pressurePa != 100000u) {
        err << "Expected 100000 Pa after reconnect" << Qt::endl;
        return false;
    }
    out << "Reconnect OK" << Qt::endl;

    // Оба запроса идут через один DeviceClient и один сокет.
    if (client.readSample().status != ExchangeStatus::Timeout) {
        err << "Expected timeout for the old request" << Qt::endl;
        return false;
    }

    const auto current = client.readSample();
    if (current.status != ExchangeStatus::Ok || current.pressurePa != 22222u) {
        err << "Expected 22222 Pa for the current request" << Qt::endl;
        return false;
    }
    out << "Late reply ignored OK" << Qt::endl;

    socket.disconnectFromHost();
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::promise<quint16> portReady;
    auto port = portReady.get_future();
    bool serverOk = false;

    std::thread serverThread([&] {
        serverOk = runServer(portReady);
    });

    const bool clientOk = runClient(port.get());
    serverThread.join();

    if (!serverOk) {
        QTextStream(stderr) << "Test server did not finish its scenario"
                            << Qt::endl;
    }

    return clientOk && serverOk ? 0 : 1;
}