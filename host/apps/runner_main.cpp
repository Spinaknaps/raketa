#include "test_scenario.h"
#include "device_client.h"
#include "test_runner.h"
#include "test_outcome.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QStringList>
#include <QTcpSocket>
#include <QTextStream>

#include <optional>

namespace {

std::optional<std::uint8_t> parseMode(const QString &name)
{
    if (name == QStringLiteral("normal")) {
        return 0u;
    }
    if (name == QStringLiteral("stuck")) {
        return 1u;
    }
    if (name == QStringLiteral("silent")) {
        return 2u;
    }
    return std::nullopt;
}

void printUsage(QTextStream &err)
{
    err << "Usage: raketa_runner <normal|stuck|silent>" << Qt::endl;
}

void printReport(QTextStream &out, const QString &modeName, const RunResult &run)
{
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
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const auto args = QCoreApplication::arguments();
    if (args.size() != 2) {
        printUsage(err);
        return 2;
    }

    const auto &modeName = args[1];
    const auto mode = parseMode(modeName);
    if (!mode) {
        err << "Unknown mode: " << modeName << Qt::endl;
        printUsage(err);
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
    const auto run = runPressureTest(client, *mode, pressureScenario);
    printReport(out, modeName, run);
    return run.outcome == TestOutcome::Pass ? 0 : 1;
}
