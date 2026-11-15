#include "device_client.h"
#include "test_runner.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QStringList>
#include <QTcpSocket>
#include <QTextStream>

static const char *outcomeName(TestOutcome outcome)
{
    switch (outcome) {
    case TestOutcome::Pass:            return "PASS";
    case TestOutcome::Fail:            return "FAIL";
    case TestOutcome::Timeout:         return "TIMEOUT";
    case TestOutcome::CommandError:    return "COMMAND_ERROR";
    case TestOutcome::ConnectionError: return "CONNECTION_ERROR";
    case TestOutcome::Cancelled:       return "CANCELLED";
    }
    return "UNKNOWN";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const auto args = app.arguments();
    if (args.size() != 2) {
        err << "Usage: raketa_runner <normal|stuck|silent>" << Qt::endl;
        return 2;
    }

    const auto &modeName = args[1];
    std::uint8_t mode;
    if (modeName == "normal") {
        mode = 0u;
    } else if (modeName == "stuck") {
        mode = 1u;
    } else if (modeName == "silent") {
        mode = 2u;
    } else {
        err << "Unknown mode: " << modeName << Qt::endl;
        err << "Usage: raketa_runner <normal|stuck|silent>" << Qt::endl;
        return 2;
    }

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, 5555);
    if (!socket.waitForConnected(3000)) {
        err << "Result: CONNECTION_ERROR (" << socket.errorString()
            << ')' << Qt::endl;
        return 1;
    }

    DeviceClient client(socket);
    const auto run = runPressureTest(client, mode, pressureScenario);

    out << "Mode: " << modeName << Qt::endl;
    for (const auto &step : run.steps) {
        out << "Step " << static_cast<qulonglong>(step.stepNumber)
            << ": expected " << step.expectedPa
            << " Pa, tolerance +/-" << step.tolerancePa << " Pa, ";

        if (step.measuredPa) {
            out << "got " << *step.measuredPa << " Pa";
        } else {
            out << "no measurement";
        }

        out << ", " << outcomeName(step.outcome) << Qt::endl;
    }

    out << "Result: " << outcomeName(run.outcome);
    if (!run.detail.empty()) {
        out << " (" << QString::fromStdString(run.detail) << ')';
    }
    out << Qt::endl;

    return run.outcome == TestOutcome::Pass ? 0 : 1;
}