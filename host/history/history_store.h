#pragma once

#include "history_run_summary.h"
#include "run_metadata.h"
#include "run_result.h"

#include <QString>
#include <array>
#include <optional>
#include <vector>

class QSqlDatabase;

bool saveHistoryRun(
    QSqlDatabase &db,
    const RunMetadata &metadata,
    const RunResult &run,
    QString &error
);

std::optional<std::vector<HistoryRunSummary>> readHistoryRuns(QSqlDatabase &db, QString &error);
std::optional<std::vector<std::array<QString, 6>>> readHistorySteps(
    QSqlDatabase &db, qlonglong runId, QString &error
);
