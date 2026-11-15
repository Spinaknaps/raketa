#include "dashboard_view.h"
#include "pressure_chart.h"
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QCloseEvent>


namespace {

QHBoxLayout *buildControls(DashboardView &view)
{
    // Адрес, порт, режим и кнопки.
    auto *controls = new QHBoxLayout;

    view.address = new QLineEdit(QStringLiteral("127.0.0.1"));
    view.port = new QSpinBox;
    view.port->setRange(1, 65535);
    view.port->setValue(5555);

    view.mode = new QComboBox;
    view.mode->addItem(QStringLiteral("normal"), 0);
    view.mode->addItem(QStringLiteral("stuck"), 1);
    view.mode->addItem(QStringLiteral("silent"), 2);


    view.start = new QPushButton(QStringLiteral("Запустить"));
    view.cancel = new QPushButton(QStringLiteral("Отменить"));
    view.cancel->setEnabled(false);

    controls->addWidget(new QLabel(QStringLiteral("Адрес")));
    controls->addWidget(view.address, 1);
    controls->addWidget(new QLabel(QStringLiteral("Порт")));
    controls->addWidget(view.port);
    controls->addWidget(new QLabel(QStringLiteral("Режим")));
    controls->addWidget(view.mode);
    controls->addWidget(view.start);
    controls->addWidget(view.cancel);
    view.history = new QPushButton(QStringLiteral("История"));
    controls->addWidget(view.history);
    return controls;
}

QHBoxLayout *buildScenarioPicker(DashboardView &view)
{
    auto *scenarioControls = new QHBoxLayout;
    view.chooseScenario = new QPushButton(QStringLiteral("Выбрать Tcl-файл…"));
    view.scenarioLabel = new QLabel(QStringLiteral("Сценарий не выбран"));
    view.scenarioLabel->setTextFormat(Qt::PlainText);

    scenarioControls->addWidget(view.chooseScenario);
    scenarioControls->addWidget(view.scenarioLabel, 1);
    return scenarioControls;
}

QTableWidget *buildStepTable()
{
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
    return steps;
}

QGroupBox *buildChart(DashboardView &view)
{
    auto *chart = new QGroupBox(QStringLiteral("График давления"));
    auto *chartLayout = new QVBoxLayout(chart);
    view.pressureChart = new PressureChart(chart);
    chartLayout->addWidget(view.pressureChart);
    return chart;
}

} // namespace

DashboardView::DashboardView()
{
    setWindowTitle(QStringLiteral("raketa — стенд испытаний"));
    resize(900, 650);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    layout->addLayout(buildControls(*this));
    layout->addLayout(buildScenarioPicker(*this));

    status = new QLabel(QStringLiteral("Статус: ожидание запуска"));
    layout->addWidget(status);

    steps = buildStepTable();
    layout->addWidget(steps, 2);
    layout->addWidget(buildChart(*this), 2);

    layout->addWidget(new QLabel(QStringLiteral("Журнал")));
    log = new QPlainTextEdit;
    log->setReadOnly(true);
    layout->addWidget(log, 1);
}

void DashboardView::closeEvent(QCloseEvent *event)
{
    if (canClose && !canClose()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}
