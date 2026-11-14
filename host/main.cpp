#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTcpSocket socket;
    QTimer deadline;
    QByteArray incoming;
    QTextStream out(stdout);
    QTextStream err(stderr);

    deadline.setSingleShot(true);

    QObject::connect(&deadline, &QTimer::timeout, &app, [&] {
        err << "TIMEOUT: no PONG within 5 seconds" << Qt::endl;
        app.exit(1);
    });

    QObject::connect(&socket, &QTcpSocket::connected, &app, [&] {
        out << "Connected to 127.0.0.1:5555" << Qt::endl;

        if (socket.write("PING\n") != 5) {
            err << "Could not queue PING for sending" << Qt::endl;
            app.exit(1);
            return;
        }

        out << "TX PING" << Qt::endl;
    });

    QObject::connect(&socket, &QTcpSocket::readyRead, &app, [&] {
        incoming += socket.readAll();

        while (true) {
            const auto newline = incoming.indexOf('\n');
            if (newline < 0) {
                break;
            }

            QByteArray line = incoming.left(newline);
            incoming.remove(0, newline + 1);

            if (line.endsWith('\r')) {
                line.chop(1);
            }

            if (line == "PONG") {
                deadline.stop();
                out << "RX PONG" << Qt::endl;
                app.exit(0);
                return;
            }
        }
    });

    QObject::connect(&socket, &QTcpSocket::errorOccurred, &app,
                     [&](QAbstractSocket::SocketError) {
        err << "TCP error: " << socket.errorString() << Qt::endl;
        app.exit(1);
    });

    QTimer::singleShot(0, &app, [&] {
        deadline.start(5000);
        socket.connectToHost(QHostAddress::LocalHost, 5555);
    });

    return app.exec();
}