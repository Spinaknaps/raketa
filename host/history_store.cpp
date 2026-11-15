#include "history_store.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QString outcomeForStorage(TestOutcome outcome)
{
    switch (outcome) {
    case TestOutcome::Pass:            return QStringLiteral("PASS");
    case TestOutcome::Fail:            return QStringLiteral("FAIL");
    case TestOutcome::Timeout:         return QStringLiteral("TIMEOUT");
    case TestOutcome::CommandError:    return QStringLiteral("COMMAND_ERROR");
    case TestOutcome::ConnectionError: return QStringLiteral("CONNECTION_ERROR");
    case TestOutcome::Cancelled:       return QStringLiteral("CANCELLED");
    }

    return QStringLiteral("UNKNOWN");
}

static bool insertRunAndSteps(
    QSqlDatabase &db,
    const QString &startedAtUtc,
    const QString &scenario,
    const QString &mode,
    const RunResult &run,
    QString &error
)
{
    QSqlQuery insertRun(db);

    if (!insertRun.prepare(QStringLiteral(R"sql(
        INSERT INTO runs (started_at_utc, scenario, mode, outcome, detail)
        VALUES (:started_at, :scenario, :mode, :outcome, :detail)
    )sql"))) {
        error = insertRun.lastError().text();
        return false;
    }

    insertRun.bindValue(QStringLiteral(":started_at"), startedAtUtc);
    insertRun.bindValue(QStringLiteral(":scenario"), scenario);
    insertRun.bindValue(QStringLiteral(":mode"), mode);
    insertRun.bindValue(QStringLiteral(":outcome"), outcomeForStorage(run.outcome));
    insertRun.bindValue(QStringLiteral(":detail"), QString::fromStdString(run.detail));

    if (!insertRun.exec()) {
        error = QStringLiteral("runs: ") + insertRun.lastError().text();
        return false;
    }

    bool idOk = false;
    const qlonglong runId = insertRun.lastInsertId().toLongLong(&idOk);
    if (!idOk) {
        error = QStringLiteral("Не удалось получить ID прогона");
        return false;
    }

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

    for (const auto &step : run.steps) {
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
                             outcomeForStorage(step.outcome));
        insertStep.bindValue(QStringLiteral(":detail"),
                             QString::fromStdString(step.detail));

        if (!insertStep.exec()) {
            error = QStringLiteral("step_results: ") + insertStep.lastError().text();
            return false;
        }
    }

    return true;
}

bool saveHistoryRun(
    QSqlDatabase &db,
    const QString &startedAtUtc,
    const QString &scenario,
    const QString &mode,
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

    if (!insertRunAndSteps(db, startedAtUtc, scenario, mode, run, error)) {
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