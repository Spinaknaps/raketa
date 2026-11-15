#include "history_schema.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

bool createTables(QSqlQuery &query)
{
    return query.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS runs (
            id             INTEGER PRIMARY KEY,
            started_at_utc TEXT NOT NULL,
            scenario       TEXT NOT NULL,
            mode           TEXT NOT NULL,
            outcome        TEXT NOT NULL,
            detail         TEXT NOT NULL
        )
    )sql")) && query.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS step_results (
            run_id      INTEGER NOT NULL,
            step_number INTEGER NOT NULL,
            expected_pa INTEGER NOT NULL,
            tolerance_pa INTEGER NOT NULL,
            measured_pa INTEGER,
            outcome     TEXT NOT NULL,
            detail      TEXT NOT NULL,
            PRIMARY KEY (run_id, step_number),
            FOREIGN KEY (run_id) REFERENCES runs(id) ON DELETE CASCADE
        )
    )sql"));
}

} // namespace

bool initializeHistorySchema(QSqlDatabase &db, QString &error)
{
    QSqlQuery query(db);

    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        error = query.lastError().text();
        return false;
    }

    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }

    if (!createTables(query)) {
        error = query.lastError().text();
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

