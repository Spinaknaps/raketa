#include "run_worker.h"
#include "device_client.h"
#include <QTcpSocket>

RunResult executeRun(const QString &host, quint16 port, std::uint8_t mode,
                     const std::vector<PressureStep> &steps,
                     const std::atomic_bool &cancelRequested,
                     const std::function<void(const StepResult &)> &onStep)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    const bool connected = socket.waitForConnected(3000);
    if (cancelRequested.load()) {
        return {TestOutcome::Cancelled, {}, "Cancelled during connection"};
    }
    if (!connected) {
        return {TestOutcome::ConnectionError, {}, socket.errorString().toStdString()};
    }
    DeviceClient client(socket, &cancelRequested);
    return runPressureTest(client, mode, steps, onStep);
}
