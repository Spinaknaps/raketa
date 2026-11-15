#include "test_scenario.h"
#include "device_client.h"
#include "test_runner.h"

#include <chrono>
#include <thread>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTextStream>


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, 5555);
    if (!socket.waitForConnected(3000)) {
        err << "Connection failed: " << socket.errorString() << Qt::endl;
        return 1;
    }

    std::atomic_bool cancelRequested{false};
    DeviceClient client(socket, &cancelRequested);

    if (client.setMode(0u).status != ExchangeStatus::Ok ||
        client.setStimulus(100000u).status != ExchangeStatus::Ok) {
        err << "Could not prepare normal mode" << Qt::endl;
        return 1;
        }

    const auto sample = client.readSample();
    if (sample.status != ExchangeStatus::Ok || sample.pressurePa != 100000u) {
        err << "Expected a sample of 100000 Pa" << Qt::endl;
        return 1;
    }
    out << "Pressure: " << *sample.pressurePa << " Pa" << Qt::endl;

    if (client.setMode(2u).status != ExchangeStatus::Ok) {
        err << "Could not select silent mode" << Qt::endl;
        return 1;
    }

    const auto rejected = client.readSample();
    if (rejected.status != ExchangeStatus::CommandError ||
        rejected.errorCode != 0x04u) {
        err << "Expected command error 0x04" << Qt::endl;
        return 1;
        }
    out << "Command error 0x04 OK" << Qt::endl;

    if (client.setStimulus(100000u).status != ExchangeStatus::Ok ||
        client.readSample().status != ExchangeStatus::Timeout) {
        err << "Expected a read timeout in silent mode" << Qt::endl;
        return 1;
        }
    out << "Read timeout OK" << Qt::endl;

    const auto &secondStep = pressureScenario[1];
    if (!isWithinTolerance(secondStep, 89000u) ||
        !isWithinTolerance(secondStep, 91000u) ||
        isWithinTolerance(secondStep, 88999u) ||
        isWithinTolerance(secondStep, 91001u)) {
        err << "Tolerance boundaries failed" << Qt::endl;
        return 1;
        }
    out << "Tolerance boundaries OK" << Qt::endl;

    const auto normal = runPressureTest(client, 0u, pressureScenario);
    if (normal.outcome != TestOutcome::Pass || normal.steps.size() != 3u) {
        err << "Expected PASS after three steps" << Qt::endl;
        return 1;
    }
    out << "Runner normal: PASS" << Qt::endl;

    const std::vector<PressureStep> singleStep{pressureScenario.front()};
    const auto single = runPressureTest(client, 0u, singleStep);

    if (single.outcome != TestOutcome::Pass ||
        single.steps.size() != 1u ||
        single.steps.front().measuredPa != 100000u) {
        err << "Expected PASS after one step with 100000 Pa" << Qt::endl;
        return 1;
        }

    out << "Runner single step: PASS" << Qt::endl;
    
    const auto stuck = runPressureTest(client, 1u, pressureScenario);
    if (stuck.outcome != TestOutcome::Fail || stuck.steps.size() != 2u ||
        stuck.steps.back().measuredPa != 100000u) {
        err << "Expected FAIL at step 2 with 100000 Pa" << Qt::endl;
        return 1;
        }
    out << "Runner stuck: FAIL at step 2" << Qt::endl;

    const auto silent = runPressureTest(client, 2u, pressureScenario);
    if (silent.outcome != TestOutcome::Timeout || silent.steps.size() != 1u ||
        silent.steps.front().measuredPa.has_value()) {
        err << "Expected TIMEOUT at step 1 without a measurement" << Qt::endl;
        return 1;
        }
    out << "Runner silent: TIMEOUT at step 1" << Qt::endl;

    if (client.setMode(2u).status != ExchangeStatus::Ok ||
    client.setStimulus(100000u).status != ExchangeStatus::Ok) {
        err << "Could not prepare cancellation check" << Qt::endl;
        return 1;
    }

    std::thread canceller([&cancelRequested] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        cancelRequested.store(true);
    });

    const auto cancelledRead = client.readSample();
    canceller.join();
    cancelRequested.store(false);

    if (cancelledRead.status != ExchangeStatus::Cancelled) {
        err << "Expected cancellation while waiting for a sample" << Qt::endl;
        return 1;
    }
    out << "Read cancellation OK" << Qt::endl;

    socket.disconnectFromHost();
    if (client.readSample().status != ExchangeStatus::ConnectionError) {
        err << "Expected a connection error after disconnect" << Qt::endl;
        return 1;
    }
    out << "Connection error OK" << Qt::endl;
    return 0;
}