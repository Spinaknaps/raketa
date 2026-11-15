#pragma once
#include "test_runner.h"
#include <QString>
#include <atomic>

RunResult executeRun(const QString &host, quint16 port, std::uint8_t mode,
                     const std::vector<PressureStep> &steps,
                     const std::atomic_bool &cancelRequested,
                     const std::function<void(const StepResult &)> &onStep);
