#include "device_client.h"
#include "test_runner.h"
#include "history_store.h"
#include "scenario_loader.h"

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QTcpSocket>
#include <QThread>
#include <QMetaObject>
#include <QPainter>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QDateTime>
#include <QDialog>
#include <QMessageBox>
#include <QVariant>
#include <QFileDialog>

#include <tcl.h>

#include <algorithm>
#include <vector>
#include <atomic>
#include <functional>


class PressureChart : public QWidget {
public:
    explicit PressureChart(QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(320, 150);
    }

    void reset(std::size_t stepCount) {
        stepCount_ = stepCount;
        steps_.clear();
        update();
    }

    void appendStep(const StepResult &step) {
        steps_.push_back(step);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF plot(65, 28, width() - 85, height() - 58);
        const QColor expectedColor("#3486c9");
        const QColor measuredColor("#d17a20");

        painter.setPen(expectedColor);
        painter.drawText(65, 18, QStringLiteral("Эталон"));
        painter.setPen(measuredColor);
        painter.drawText(145, 18, QStringLiteral("Измерение"));

        std::uint32_t upperPa = 100000u;
        for (const auto &step: steps_) {
            upperPa = std::max(upperPa, step.expectedPa);
            if (step.measuredPa) {
                upperPa = std::max(upperPa, *step.measuredPa);
            }
        }

        painter.setPen(palette().color(QPalette::Text));
        painter.drawLine(plot.topLeft(), plot.bottomLeft());
        painter.drawLine(plot.bottomLeft(), plot.bottomRight());
        painter.drawText(4, 32, QStringLiteral("%1 Па").arg(upperPa));
        painter.drawText(4, static_cast<int>(plot.bottom()), QStringLiteral("0"));

        if (stepCount_ == 0u) {
            return;
        }

        const auto xForStep = [&](std::size_t number) {
            return plot.left() + plot.width() *
                   (static_cast<double>(number) - 0.5) /
                   static_cast<double>(stepCount_);
        };

        for (std::size_t number = 1; number <= stepCount_; ++number) {
            painter.drawText(
                QPointF(xForStep(number) - 4, plot.bottom() + 18),
                QString::number(static_cast<qulonglong>(number))
            );
        }

        const auto drawSeries = [&](const QColor &color, bool measured) {
            painter.setPen(QPen(color, 2));
            painter.setBrush(color);

            std::optional<QPointF> previous;
            for (const auto &step: steps_) {
                const auto value = measured
                                       ? step.measuredPa
                                       : std::optional<std::uint32_t>{step.expectedPa};

                if (!value) {
                    previous.reset();
                    continue;
                }

                const QPointF point(
                    xForStep(step.stepNumber),
                    plot.bottom() - plot.height() *
                    static_cast<double>(*value) / upperPa
                );

                if (previous) {
                    painter.drawLine(*previous, point);
                }
                painter.drawEllipse(point, 3, 3);
                previous = point;
            }
        };

        drawSeries(expectedColor, false);
        drawSeries(measuredColor, true);
    }

private:
    std::vector<StepResult> steps_;
    std::size_t stepCount_ = 0u;
};

static QString outcomeText(TestOutcome outcome) {
    switch (outcome) {
        case TestOutcome::Pass: return QStringLiteral("PASS");
        case TestOutcome::Fail: return QStringLiteral("FAIL");
        case TestOutcome::Timeout: return QStringLiteral("TIMEOUT");
        case TestOutcome::CommandError: return QStringLiteral("COMMAND_ERROR");
        case TestOutcome::ConnectionError: return QStringLiteral("CONNECTION_ERROR");
        case TestOutcome::Cancelled: return QStringLiteral("CANCELLED");
    }

    return QStringLiteral("UNKNOWN");
}

class DashboardWindow : public QMainWindow {
public:
    std::function<bool()> canClose;

protected:
    void closeEvent(QCloseEvent *event) override {
        if (canClose && !canClose()) {
            event->ignore();
            return;
        }

        QMainWindow::closeEvent(event);
    }
};

static bool initializeHistorySchema(QSqlDatabase &db, QString &error) {
    QSqlQuery query(db);

    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        error = query.lastError().text();
        return false;
    }

    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }

    const bool created = query.exec(QStringLiteral(R"sql(
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

    if (!created) {
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

static void showHistory(QSqlDatabase &db, QWidget *parent) {
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

    {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral(R"sql(
            SELECT id, started_at_utc, scenario, mode, outcome, detail
            FROM runs
            ORDER BY started_at_utc DESC, id DESC
        )sql"))) {
            QMessageBox::warning(
                parent, QStringLiteral("Ошибка чтения истории"),
                query.lastError().text()
            );
            return;
        }

        while (query.next()) {
            const int row = runs->rowCount();
            runs->insertRow(row);

            auto *time = new QTableWidgetItem(query.value(1).toString());
            time->setData(Qt::UserRole, query.value(0));
            time->setData(Qt::UserRole + 1, query.value(5));
            runs->setItem(row, 0, time);

            for (int column = 1; column < 4; ++column) {
                runs->setItem(
                    row, column,
                    new QTableWidgetItem(query.value(column + 1).toString())
                );
            }
        }
    }

    QObject::connect(
        runs, &QTableWidget::currentCellChanged, &dialog,
        [&](int row, int, int, int) {
            runSteps->setRowCount(0);

            const auto *time = row >= 0 ? runs->item(row, 0) : nullptr;
            if (time == nullptr) {
                detail->setText(QStringLiteral("Выбери прогон."));
                return;
            }

            const QString reason = time->data(Qt::UserRole + 1).toString();
            detail->setText(reason.isEmpty()
                                ? QStringLiteral("Причина завершения отсутствует.")
                                : QStringLiteral("Причина: %1").arg(reason));

            QSqlQuery query(db);
            if (!query.prepare(QStringLiteral(R"sql(
                SELECT step_number, expected_pa, tolerance_pa,
                       measured_pa, outcome, detail
                FROM step_results
                WHERE run_id = :run_id
                ORDER BY step_number
            )sql"))) {
                detail->setText(query.lastError().text());
                return;
            }

            query.bindValue(QStringLiteral(":run_id"), time->data(Qt::UserRole));
            if (!query.exec()) {
                detail->setText(query.lastError().text());
                return;
            }

            while (query.next()) {
                const int stepRow = runSteps->rowCount();
                runSteps->insertRow(stepRow);

                for (int column = 0; column < 6; ++column) {
                    const QVariant value = query.value(column);
                    runSteps->setItem(
                        stepRow, column,
                        new QTableWidgetItem(value.isNull() ? QString{} : value.toString())
                    );
                }
            }
        }
    );

    if (runs->rowCount() == 0) {
        detail->setText(QStringLiteral("Сохранённых прогонов пока нет."));
    } else {
        runs->setCurrentCell(0, 0);
    }

    dialog.exec();
}

int main(int argc, char *argv[]) {
    Tcl_FindExecutable(argv[0]);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("raketa"));

    DashboardWindow window;
    window.setWindowTitle(QStringLiteral("raketa — стенд испытаний"));
    window.resize(900, 650);

    auto *central = new QWidget(&window);
    window.setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    // Адрес, порт, режим и кнопки.
    auto *controls = new QHBoxLayout;

    auto *address = new QLineEdit(QStringLiteral("127.0.0.1"));
    auto *port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(5555);

    auto *mode = new QComboBox;
    mode->addItem(QStringLiteral("normal"), 0);
    mode->addItem(QStringLiteral("stuck"), 1);
    mode->addItem(QStringLiteral("silent"), 2);


    auto *start = new QPushButton(QStringLiteral("Запустить"));
    auto *cancel = new QPushButton(QStringLiteral("Отменить"));
    cancel->setEnabled(false);

    controls->addWidget(new QLabel(QStringLiteral("Адрес")));
    controls->addWidget(address, 1);
    controls->addWidget(new QLabel(QStringLiteral("Порт")));
    controls->addWidget(port);
    controls->addWidget(new QLabel(QStringLiteral("Режим")));
    controls->addWidget(mode);
    controls->addWidget(start);
    controls->addWidget(cancel);
    auto *history = new QPushButton(QStringLiteral("История"));
    controls->addWidget(history);
    layout->addLayout(controls);
    auto *scenarioControls = new QHBoxLayout;
    auto *chooseScenario = new QPushButton(QStringLiteral("Выбрать Tcl-файл…"));
    auto *scenarioLabel = new QLabel(QStringLiteral("Сценарий не выбран"));
    scenarioLabel->setTextFormat(Qt::PlainText);

    scenarioControls->addWidget(chooseScenario);
    scenarioControls->addWidget(scenarioLabel, 1);
    layout->addLayout(scenarioControls);

    QString scenarioPath;

    QObject::connect(chooseScenario, &QPushButton::clicked, &window, [&] {
        const QString selectedPath = QFileDialog::getOpenFileName(
            &window,
            QStringLiteral("Выбрать сценарий испытания"),
            scenarioPath,
            QStringLiteral("Сценарии Tcl (*.tcl)")
        );

        if (!selectedPath.isEmpty()) {
            scenarioPath = selectedPath;
            scenarioLabel->setText(scenarioPath);
        }
    });

    auto *status = new QLabel(QStringLiteral("Статус: ожидание запуска"));
    layout->addWidget(status);

    // Таблица заполнится результатами испытания.
    auto *steps = new QTableWidget(0, 5);
    steps->setHorizontalHeaderLabels({
        QStringLiteral("Ступень"),
        QStringLiteral("Эталон, Па"),
        QStringLiteral("Допуск, Па"),
        QStringLiteral("Измерение, Па"),
        QStringLiteral("Итог")
    });
    steps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    steps->verticalHeader()->hide();
    steps->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(steps, 2);

    auto *chart = new QGroupBox(QStringLiteral("График давления"));
    auto *chartLayout = new QVBoxLayout(chart);
    auto *pressureChart = new PressureChart(chart);
    chartLayout->addWidget(pressureChart);
    layout->addWidget(chart, 2);

    layout->addWidget(new QLabel(QStringLiteral("Журнал")));
    auto *log = new QPlainTextEdit;
    log->setReadOnly(true);
    layout->addWidget(log, 1);

    QSqlDatabase historyDb;

    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation
    );

    if (dataDir.isEmpty() || !QDir().mkpath(dataDir)) {
        log->appendPlainText(
            QStringLiteral("SQLite: не удалось создать каталог данных: %1")
            .arg(dataDir)
        );
    } else if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        log->appendPlainText(
            QStringLiteral("SQLite: драйвер QSQLITE недоступен")
        );
    } else {
        const QString dbPath = QDir(dataDir).filePath(
            QStringLiteral("history.sqlite")
        );

        historyDb = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QStringLiteral("raketa_history")
        );
        historyDb.setDatabaseName(dbPath);

        if (historyDb.open()) {
            QString schemaError;

            if (initializeHistorySchema(historyDb, schemaError)) {
                log->appendPlainText(
                    QStringLiteral("SQLite: таблицы истории готовы — %1").arg(dbPath)
                );
            } else {
                log->appendPlainText(
                    QStringLiteral("SQLite: ошибка создания таблиц — %1")
                    .arg(schemaError)
                );
                historyDb.close();
            }
        } else {
            log->appendPlainText(
                QStringLiteral("SQLite: не удалось открыть %1 — %2")
                .arg(dbPath, historyDb.lastError().text())
            );
        }
    }

    QObject::connect(history, &QPushButton::clicked, &window, [&] {
        showHistory(historyDb, &window);
    });

    QThread *activeThread = nullptr;
    std::atomic_bool cancelRequested{false};
    bool closePending = false;
    RunResult runResult{TestOutcome::ConnectionError, {}, {}};

    const auto showStep = [steps, pressureChart, log](const StepResult &step) {
        const int row = steps->rowCount();
        steps->insertRow(row);

        const QString number =
                QString::number(static_cast<qulonglong>(step.stepNumber));
        const QString outcome = outcomeText(step.outcome);
        const QString measured = step.measuredPa
                                     ? QString::number(*step.measuredPa)
                                     : QString{};

        steps->setItem(row, 0, new QTableWidgetItem(number));
        steps->setItem(row, 1, new QTableWidgetItem(QString::number(step.expectedPa)));
        steps->setItem(row, 2, new QTableWidgetItem(QString::number(step.tolerancePa)));
        steps->setItem(row, 3, new QTableWidgetItem(measured));
        steps->setItem(row, 4, new QTableWidgetItem(outcome));

        pressureChart->appendStep(step);

        QString message = QStringLiteral("Ступень %1 — %2").arg(number, outcome);
        if (!step.detail.empty()) {
            message += QStringLiteral(" — ") + QString::fromStdString(step.detail);
        }
        log->appendPlainText(message);
    };

    const auto requestCancellation = [&] {
        if (activeThread == nullptr || cancelRequested.exchange(true)) {
            return;
        }

        cancel->setEnabled(false);
        status->setText(QStringLiteral("Отмена испытания…"));
        log->appendPlainText(QStringLiteral("Запрошена отмена"));
    };

    QObject::connect(cancel, &QPushButton::clicked, &window, requestCancellation);

    window.canClose = [&] {
        if (activeThread == nullptr) {
            return true;
        }

        closePending = true;
        requestCancellation();
        return false;
    };

    QObject::connect(start, &QPushButton::clicked, &window, [&] {
        const QString host = address->text().trimmed();
        if (host.isEmpty()) {
            status->setText(QStringLiteral("Введите адрес прибора"));
            return;
        }

        const auto tcpPort = static_cast<quint16>(port->value());
        const auto selectedMode =
                static_cast<std::uint8_t>(mode->currentData().toInt());

        if (scenarioPath.isEmpty()) {
            status->setText(QStringLiteral("Сначала выбери Tcl-сценарий"));
            return;
        }

        LoadedScenario loaded;
        std::string loadError;
        const auto pathUtf8 = scenarioPath.toUtf8();

        if (!loadScenario(pathUtf8.constData(), loaded, loadError)) {
            scenarioLabel->setText(scenarioPath);
            status->setText(QStringLiteral("Ошибка сценария"));
            log->appendPlainText(QString::fromUtf8(loadError.c_str()));
            return;
        }

        const QString scenarioName = QString::fromUtf8(loaded.name.c_str());
        scenarioLabel->setText(scenarioName);
        scenarioLabel->setToolTip(scenarioPath);

        const QString startedAtUtc =
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        const QString runMode = mode->currentText();

        cancelRequested.store(false);
        cancel->setEnabled(true);
        start->setEnabled(false);
        address->setEnabled(false);
        port->setEnabled(false);
        mode->setEnabled(false);
        status->setText(QStringLiteral("Идёт испытание…"));
        steps->setRowCount(0);
        pressureChart->reset(loaded.steps.size());
        chooseScenario->setEnabled(false);        log->clear();
        log->appendPlainText(QStringLiteral("Запуск испытания"));

        auto *thread = QThread::create([host, tcpPort, selectedMode, &runResult, &window, showStep, &cancelRequested, scenarioSteps = loaded.steps] {
            QTcpSocket socket;
            socket.connectToHost(host, tcpPort);

            const bool connected = socket.waitForConnected(3000);

            if (cancelRequested.load()) {
                runResult = {TestOutcome::Cancelled, {}, "Cancelled during connection"};
                return;
            }

            if (!connected) {
                runResult = {
                    TestOutcome::ConnectionError,
                    {},
                    socket.errorString().toStdString()
                };
                return;
            }

            DeviceClient client(socket, &cancelRequested);
            runResult = runPressureTest(
                client, selectedMode, scenarioSteps,
                [&window, showStep](const StepResult &step) {
                    QMetaObject::invokeMethod(
                        &window,
                        [showStep, step] { showStep(step); },
                        Qt::QueuedConnection
                    );
                }
            );
        });

        activeThread = thread;

        QObject::connect(thread, &QThread::finished, &window, [&, thread, startedAtUtc, runMode, scenarioName] {
            thread->wait();

            const QString summary = outcomeText(runResult.outcome);
            status->setText(QStringLiteral("Итог: %1").arg(summary));

            QString message = QStringLiteral("Итог: %1").arg(summary);
            if (!runResult.detail.empty()) {
                message += QStringLiteral(" — ") +
                        QString::fromStdString(runResult.detail);
            }
            log->appendPlainText(message);

            QString saveError;
            if (saveHistoryRun(
                historyDb,
                startedAtUtc,
                scenarioName,
                runMode,
                runResult,
                saveError
            )) {
                log->appendPlainText(QStringLiteral("SQLite: прогон сохранён"));
            } else {
                log->appendPlainText(
                    QStringLiteral("SQLite: не удалось сохранить прогон — %1")
                    .arg(saveError)
                );
            }

            activeThread = nullptr;
            thread->deleteLater();

            cancel->setEnabled(false);

            if (closePending) {
                window.close();
                return;
            }

            start->setEnabled(true);
            address->setEnabled(true);
            port->setEnabled(true);
            mode->setEnabled(true);
            chooseScenario->setEnabled(true);
        }, Qt::QueuedConnection);

        thread->start();
    });

    window.show();
    const int exitCode = QApplication::exec();

    if (activeThread != nullptr) {
        activeThread->wait();
        delete activeThread;
    }

    Tcl_Finalize();

    return exitCode;
}
