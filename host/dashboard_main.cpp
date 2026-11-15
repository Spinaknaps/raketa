#include "device_client.h"
#include "test_runner.h"

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

    void clear() {
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

        const auto xForStep = [&](std::size_t number) {
            return plot.left() + plot.width() *
                   (static_cast<double>(number) - 0.5) / pressureScenario.size();
        };

        for (std::size_t number = 1; number <= pressureScenario.size(); ++number) {
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

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
    layout->addLayout(controls);

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

        cancelRequested.store(false);
        cancel->setEnabled(true);
        start->setEnabled(false);
        address->setEnabled(false);
        port->setEnabled(false);
        mode->setEnabled(false);
        status->setText(QStringLiteral("Идёт испытание…"));
        steps->setRowCount(0);
        pressureChart->clear();
        log->clear();
        log->appendPlainText(QStringLiteral("Запуск испытания"));

        auto *thread = QThread::create([host, tcpPort, selectedMode, &runResult, &window, showStep, &cancelRequested] {
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
                client, selectedMode, pressureScenario,
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

        QObject::connect(thread, &QThread::finished, &window, [&, thread] {
            thread->wait();

            const QString summary = outcomeText(runResult.outcome);
            status->setText(QStringLiteral("Итог: %1").arg(summary));

            QString message = QStringLiteral("Итог: %1").arg(summary);
            if (!runResult.detail.empty()) {
                message += QStringLiteral(" — ") +
                        QString::fromStdString(runResult.detail);
            }
            log->appendPlainText(message);

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
        }, Qt::QueuedConnection);

        thread->start();
    });

    window.show();
    const int exitCode = QApplication::exec();

    if (activeThread != nullptr) {
        activeThread->wait();
        delete activeThread;
    }

    return exitCode;
}
