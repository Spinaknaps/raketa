#include "history_dialog.h"
#include "history_store.h"

#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

bool populateRuns(QSqlDatabase &db, QTableWidget *runs, QWidget *parent)
{
    QString error;
    const auto savedRuns = readHistoryRuns(db, error);
    if (!savedRuns) {
        QMessageBox::warning(parent, QStringLiteral("Ошибка чтения истории"), error);
        return false;
    }

    for (const auto &run : *savedRuns) {
        const int row = runs->rowCount();
        runs->insertRow(row);

        auto *time = new QTableWidgetItem(run.metadata.startedAtUtc);
        time->setData(Qt::UserRole, run.id);
        time->setData(Qt::UserRole + 1, run.detail);
        runs->setItem(row, 0, time);
        runs->setItem(row, 1, new QTableWidgetItem(run.metadata.scenario));
        runs->setItem(row, 2, new QTableWidgetItem(run.metadata.mode));
        runs->setItem(row, 3, new QTableWidgetItem(run.outcome));
    }
    return true;
}

void showRunSteps(QSqlDatabase &db, const QTableWidgetItem *time,
                  QTableWidget *runSteps, QLabel *detail)
{
    runSteps->setRowCount(0);
    if (!time) {
        detail->setText(QStringLiteral("Выбери прогон."));
        return;
    }

    QString error;
    const auto steps = readHistorySteps(db, time->data(Qt::UserRole).toLongLong(), error);
    if (!steps) {
        detail->setText(error);
        return;
    }

    const QString reason = time->data(Qt::UserRole + 1).toString();
    detail->setText(reason.isEmpty()
        ? QStringLiteral("Причина завершения отсутствует.")
        : QStringLiteral("Причина: %1").arg(reason));

    for (const auto &step : *steps) {
        const int row = runSteps->rowCount();
        runSteps->insertRow(row);
        for (std::size_t column = 0; column < step.size(); ++column) {
            runSteps->setItem(row, static_cast<int>(column), new QTableWidgetItem(step[column]));
        }
    }
}

} // namespace

void showHistory(QSqlDatabase &db, QWidget *parent)
{
    if (!db.isOpen()) {
        QMessageBox::warning(
            parent, QStringLiteral("История"),
            QStringLiteral("База истории недоступна.")
        );
        return;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("История испытаний"));
    dialog.resize(900, 560);

    auto *layout = new QVBoxLayout(&dialog);

    auto *runs = new QTableWidget(0, 4, &dialog);
    runs->setHorizontalHeaderLabels({
        QStringLiteral("Запуск (UTC)"),
        QStringLiteral("Сценарий"),
        QStringLiteral("Режим"),
        QStringLiteral("Итог")
    });
    runs->setSelectionBehavior(QAbstractItemView::SelectRows);
    runs->setSelectionMode(QAbstractItemView::SingleSelection);
    runs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    runs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto *detail = new QLabel(
        QStringLiteral("Выбери прогон, чтобы увидеть его ступени."), &dialog
    );
    detail->setWordWrap(true);

    auto *runSteps = new QTableWidget(0, 6, &dialog);
    runSteps->setHorizontalHeaderLabels({
        QStringLiteral("Ступень"),
        QStringLiteral("Эталон, Па"),
        QStringLiteral("Допуск, Па"),
        QStringLiteral("Измерение, Па"),
        QStringLiteral("Итог"),
        QStringLiteral("Причина")
    });
    runSteps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    runSteps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addWidget(runs, 1);
    layout->addWidget(detail);
    layout->addWidget(runSteps, 1);

    if (!populateRuns(db, runs, parent)) {
        return;
    }

    QObject::connect(
        runs, &QTableWidget::currentCellChanged, &dialog,
        [&](int row, int, int, int) {
            const auto *item = row >= 0 ? runs->item(row, 0) : nullptr;
            showRunSteps(db, item, runSteps, detail);
        }
    );

    if (runs->rowCount() == 0) {
        detail->setText(QStringLiteral("Сохранённых прогонов пока нет."));
    } else {
        runs->setCurrentCell(0, 0);
    }

    dialog.exec();
}

