#pragma once

#include "dashboard_view.h"
#include "run_result.h"
#include "loaded_scenario.h"
#include "run_metadata.h"
#include <QSqlDatabase>
#include <atomic>
#include <optional>

class QThread;

class DashboardSession {
public:
    DashboardSession();
    ~DashboardSession();
    int run();

private:
    void chooseFile();
    void showStep(const StepResult &step);
    void requestCancellation();
    void startRun();
    std::optional<LoadedScenario> loadSelectedScenario();
    void prepareRun(std::size_t stepCount);
    void launchRun(const QString &host, quint16 port, std::uint8_t mode,
                   std::vector<PressureStep> steps, const RunMetadata &metadata);
    void finishRun(const RunMetadata &metadata);
    void setRunning(bool running);

    DashboardView window;
    QSqlDatabase historyDb;
    QString scenarioPath;
    QThread *activeThread = nullptr;
    std::atomic_bool cancelRequested{false};
    bool closePending = false;
    RunResult runResult{TestOutcome::ConnectionError, {}, {}};
};
