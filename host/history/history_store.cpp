#include "history_store.h"
#include "test_outcome.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <optional>

namespace {

std::optional<qlonglong> insertRun(
    QSqlDatabase &db, const RunMetadata &metadata, const RunResult &run, QString &error
)
{
    QSqlQuery insertRun(db);

    if (!insertRun.prepare(QStringLiteral(R"sql(
        INSERT INTO runs (started_at_utc, scenario, mode, outcome, detail)
        VALUES (:started_at, :scenario, :mode, :outcome, :detail)
    )sql"))) {
        error = insertRun.lastError().text();
        return std::nullopt;
    }

    insertRun.bindValue(QStringLiteral(":started_at"), metadata.startedAtUtc);
    insertRun.bindValue(QStringLiteral(":scenario"), metadata.scenario);
    insertRun.bindValue(QStringLiteral(":mode"), metadata.mode);
    insertRun.bindValue(QStringLiteral(":outcome"), QString::fromLatin1(outcomeName(run.outcome)));
    insertRun.bindValue(QStringLiteral(":detail"), QString::fromStdString(run.detail));

    if (!insertRun.exec()) {
        error = QStringLiteral("runs: ") + insertRun.lastError().text();
        return std::nullopt;
    }

    bool idOk = false;
    const qlonglong runId = insertRun.lastInsertId().toLongLong(&idOk);
    if (!idOk) {
        error = QStringLiteral("Не удалось получить ID прогона");
        return std::nullopt;
    }

    return runId;
}

bool insertSteps(QSqlDatabase &db, qlonglong runId,
                 const std::vector<StepResult> &steps, QString &error)
{
    QSqlQuery insertStep(db);

    if (!insertStep.prepare(QStringLiteral(R"sql(
        INSERT INTO step_results (
            run_id, step_number, expected_pa, tolerance_pa,
            measured_pa, outcome, detail
        ) VALUES (
            :run_id, :step_number, :expected_pa, :tolerance_pa,
            :measured_pa, :outcome, :detail
        )
    )sql"))) {
        error = insertStep.lastError().text();
        return false;
    }

    for (const auto &step : steps) {
        insertStep.bindValue(QStringLiteral(":run_id"), runId);
        insertStep.bindValue(QStringLiteral(":step_number"),
                             static_cast<qlonglong>(step.stepNumber));
        insertStep.bindValue(QStringLiteral(":expected_pa"),
                             static_cast<qlonglong>(step.expectedPa));
        insertStep.bindValue(QStringLiteral(":tolerance_pa"),
                             static_cast<qlonglong>(step.tolerancePa));
        insertStep.bindValue(
            QStringLiteral(":measured_pa"),
            step.measuredPa
                ? QVariant::fromValue(static_cast<qlonglong>(*step.measuredPa))
                : QVariant{}
        );
        insertStep.bindValue(QStringLiteral(":outcome"),
                             QString::fromLatin1(outcomeName(step.outcome)));
        insertStep.bindValue(QStringLiteral(":detail"),
                             QString::fromStdString(step.detail));

        if (!insertStep.exec()) {
            error = QStringLiteral("step_results: ") + insertStep.lastError().text();
            return false;
        }
    }

    return true;
}

} // namespace

bool saveHistoryRun(
    QSqlDatabase &db,
    const RunMetadata &metadata,
    const RunResult &run,
    QString &error
)
{
    error.clear();

    if (!db.isOpen()) {
        error = QStringLiteral("База истории закрыта");
        return false;
    }

    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }

    const auto runId = insertRun(db, metadata, run, error);
    if (!runId || !insertSteps(db, *runId, run.steps, error)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        error = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

std::optional<std::vector<HistoryRunSummary>> readHistoryRuns(QSqlDatabase &db, QString &error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(R"sql(
        SELECT id, started_at_utc, scenario, mode, outcome, detail
        FROM runs
        ORDER BY started_at_utc DESC, id DESC
    )sql"))) {
        error = query.lastError().text();
        return std::nullopt;
    }

    std::vector<HistoryRunSummary> runs;
    while (query.next()) {
        runs.push_back({
            query.value(0).toLongLong(),
            {query.value(1).toString(), query.value(2).toString(), query.value(3).toString()},
            query.value(4).toString(),
            query.value(5).toString()
        });
    }
    if (query.lastError().isValid()) {
        error = query.lastError().text();
        return std::nullopt;
    }
    return runs;
}

std::optional<std::vector<std::array<QString, 6>>> readHistorySteps(
    QSqlDatabase &db, qlonglong runId, QString &error
)
{
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(R"sql(
        SELECT step_number, expected_pa, tolerance_pa,
               measured_pa, outcome, detail
        FROM step_results
        WHERE run_id = :run_id
        ORDER BY step_number
    )sql"))) {
        error = query.lastError().text();
        return std::nullopt;
    }
    query.bindValue(QStringLiteral(":run_id"), runId);
    if (!query.exec()) {
        error = query.lastError().text();
        return std::nullopt;
    }

    std::vector<std::array<QString, 6>> steps;
    while (query.next()) {
        auto &row = steps.emplace_back();
        for (std::size_t column = 0; column < row.size(); ++column) {
            row[column] = query.value(static_cast<int>(column)).toString();
        }
    }
    if (query.lastError().isValid()) {
        error = query.lastError().text();
        return std::nullopt;
    }
    return steps;
}
