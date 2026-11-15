#include "history_database.h"
#include "history_schema.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>

namespace {

QString openSqlite(QSqlDatabase &db, const QString &path)
{
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("raketa_history"));
    db.setDatabaseName(path);
    if (!db.open()) {
        return QStringLiteral("SQLite: не удалось открыть %1 — %2")
            .arg(path, db.lastError().text());
    }

    QString error;
    if (!initializeHistorySchema(db, error)) {
        db.close();
        return QStringLiteral("SQLite: ошибка создания таблиц — %1").arg(error);
    }

    return QStringLiteral("SQLite: таблицы истории готовы — %1").arg(path);
}

} // namespace

QString openHistoryDatabase(QSqlDatabase &historyDb)
{
    const auto dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation
    );
    if (dataDir.isEmpty() || !QDir().mkpath(dataDir)) {
        return QStringLiteral("SQLite: не удалось создать каталог данных: %1").arg(dataDir);
    }
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        return QStringLiteral("SQLite: драйвер QSQLITE недоступен");
    }

    return openSqlite(historyDb, QDir(dataDir).filePath(QStringLiteral("history.sqlite")));
}
