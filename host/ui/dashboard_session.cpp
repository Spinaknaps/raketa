#include "dashboard_session.h"
#include "history_database.h"
#include "history_dialog.h"
#include "history_store.h"
#include "test_outcome.h"
#include "pressure_chart.h"
#include "scenario_loader.h"
#include "run_worker.h"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QThread>
#include <QMetaObject>
#include <QDateTime>
#include <QFileDialog>


#include <QApplication>

#include <utility>

DashboardSession::DashboardSession()
{
    window.log->appendPlainText(openHistoryDatabase(historyDb));
    QObject::connect(window.chooseScenario, &QPushButton::clicked, &window,
                     [this] { chooseFile(); });
    QObject::connect(window.start, &QPushButton::clicked, &window,
                     [this] { startRun(); });
    QObject::connect(window.cancel, &QPushButton::clicked, &window,
                     [this] { requestCancellation(); });
    QObject::connect(window.history, &QPushButton::clicked, &window,
                     [this] { showHistory(historyDb, &window); });
    window.canClose = [this] {
        if (!activeThread) {
            return true;
        }
        closePending = true;
        requestCancellation();
        return false;
    };
}

DashboardSession::~DashboardSession()
{
    if (activeThread) {
        cancelRequested.store(true);
        activeThread->wait();
        delete activeThread;
    }
}

int DashboardSession::run()
{
    window.show();
    return QApplication::exec();
}

void DashboardSession::chooseFile()
{
    const QString selectedPath = QFileDialog::getOpenFileName(
        &window,
        QStringLiteral("Выбрать сценарий испытания"),
        scenarioPath,
        QStringLiteral("Сценарии Tcl (*.tcl)")
    );

    if (!selectedPath.isEmpty()) {
        scenarioPath = selectedPath;
        window.scenarioLabel->setText(scenarioPath);
    }
}

void DashboardSession::showStep(const StepResult &step)
{
    const int row = window.steps->rowCount();
    window.steps->insertRow(row);

    const QString number =
            QString::number(static_cast<qulonglong>(step.stepNumber));
    const QString outcome = QString::fromLatin1(outcomeName(step.outcome));
    const QString measured = step.measuredPa
                                 ? QString::number(*step.measuredPa)
                                 : QString{};

    window.steps->setItem(row, 0, new QTableWidgetItem(number));
    window.steps->setItem(row, 1, new QTableWidgetItem(QString::number(step.expectedPa)));
    window.steps->setItem(row, 2, new QTableWidgetItem(QString::number(step.tolerancePa)));
    window.steps->setItem(row, 3, new QTableWidgetItem(measured));
    window.steps->setItem(row, 4, new QTableWidgetItem(outcome));

    window.pressureChart->appendStep(step);

    QString message = QStringLiteral("Ступень %1 — %2").arg(number, outcome);
    if (!step.detail.empty()) {
        message += QStringLiteral(" — ") + QString::fromStdString(step.detail);
    }
    window.log->appendPlainText(message);
}

void DashboardSession::requestCancellation()
{
    if (activeThread == nullptr || cancelRequested.exchange(true)) {
        return;
    }

    window.cancel->setEnabled(false);
    window.status->setText(QStringLiteral("Отмена испытания…"));
    window.log->appendPlainText(QStringLiteral("Запрошена отмена"));
}

void DashboardSession::setRunning(bool running)
{
    window.start->setEnabled(!running);
    window.address->setEnabled(!running);
    window.port->setEnabled(!running);
    window.mode->setEnabled(!running);
    window.chooseScenario->setEnabled(!running);
    window.cancel->setEnabled(running);
}

void DashboardSession::startRun()
{
    const QString host = window.address->text().trimmed();
    if (host.isEmpty()) {
        window.status->setText(QStringLiteral("Введите адрес прибора"));
        return;
    }

    const auto tcpPort = static_cast<quint16>(window.port->value());
    const auto selectedMode =
            static_cast<std::uint8_t>(window.mode->currentData().toInt());

    auto loaded = loadSelectedScenario();
    if (!loaded) {
        return;
    }

    const RunMetadata metadata{
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
        QString::fromUtf8(loaded->name.c_str()),
        window.mode->currentText()
    };

    prepareRun(loaded->steps.size());
    launchRun(host, tcpPort, selectedMode, std::move(loaded->steps), metadata);
}

std::optional<LoadedScenario> DashboardSession::loadSelectedScenario()
{
    if (scenarioPath.isEmpty()) {
        window.status->setText(QStringLiteral("Сначала выбери Tcl-сценарий"));
        return std::nullopt;
    }

    LoadedScenario loaded;
    std::string loadError;
    const auto pathUtf8 = scenarioPath.toUtf8();

    if (!loadScenario(pathUtf8.constData(), loaded, loadError)) {
        window.scenarioLabel->setText(scenarioPath);
        window.status->setText(QStringLiteral("Ошибка сценария"));
        window.log->appendPlainText(QString::fromUtf8(loadError.c_str()));
        return std::nullopt;
    }

    const QString scenarioName = QString::fromUtf8(loaded.name.c_str());
    window.scenarioLabel->setText(scenarioName);
    window.scenarioLabel->setToolTip(scenarioPath);

    return loaded;
}

void DashboardSession::prepareRun(std::size_t stepCount)
{
    cancelRequested.store(false);
    setRunning(true);
    window.status->setText(QStringLiteral("Идёт испытание…"));
    window.steps->setRowCount(0);
    window.pressureChart->reset(stepCount);
    window.log->clear();
    window.log->appendPlainText(QStringLiteral("Запуск испытания"));
}

void DashboardSession::launchRun(
    const QString &host, quint16 port, std::uint8_t mode,
    std::vector<PressureStep> steps, const RunMetadata &metadata
)
{
    activeThread = QThread::create(
        [this, host, port, mode, scenarioSteps = std::move(steps)] {
            runResult = executeRun(
                host, port, mode, scenarioSteps, cancelRequested,
                [this](const StepResult &step) {
                    QMetaObject::invokeMethod(&window, [this, step] {
                        showStep(step);
                    }, Qt::QueuedConnection);
                });
        });

    QObject::connect(activeThread, &QThread::finished, &window,
                     [this, metadata] {
        finishRun(metadata);
    }, Qt::QueuedConnection);
    activeThread->start();
}

void DashboardSession::finishRun(const RunMetadata &metadata)
{
    activeThread->wait();
    const QString summary = QString::fromLatin1(outcomeName(runResult.outcome));
    window.status->setText(QStringLiteral("Итог: %1").arg(summary));

    QString message = QStringLiteral("Итог: %1").arg(summary);
    if (!runResult.detail.empty()) {
        message += QStringLiteral(" — ") +
                QString::fromStdString(runResult.detail);
    }
    window.log->appendPlainText(message);

    QString saveError;
    if (saveHistoryRun(
        historyDb,
        metadata,
        runResult,
        saveError
    )) {
        window.log->appendPlainText(QStringLiteral("SQLite: прогон сохранён"));
    } else {
        window.log->appendPlainText(
            QStringLiteral("SQLite: не удалось сохранить прогон — %1")
            .arg(saveError)
        );
    }

    delete activeThread;
    activeThread = nullptr;
    setRunning(false);
    if (closePending) {
        window.close();
    }
}
