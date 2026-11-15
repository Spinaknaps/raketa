#pragma once

#include "test_scenario.h"

#include <QString>

class QSqlDatabase;

bool saveHistoryRun(
    QSqlDatabase &db,
    const QString &startedAtUtc,
    const QString &scenario,
    const QString &mode,
    const RunResult &run,
    QString &error
);