#pragma once
class QSqlDatabase;
class QString;

bool initializeHistorySchema(QSqlDatabase &db, QString &error);
