#pragma once
#include <QMainWindow>
#include <functional>
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QTableWidget;
class PressureChart;
class QPlainTextEdit;

class DashboardView : public QMainWindow {
public:
    DashboardView();
    std::function<bool()> canClose;
    QLineEdit *address;
    QSpinBox *port;
    QComboBox *mode;
    QPushButton *start;
    QPushButton *cancel;
    QPushButton *history;
    QPushButton *chooseScenario;
    QLabel *scenarioLabel;
    QLabel *status;
    QTableWidget *steps;
    PressureChart *pressureChart;
    QPlainTextEdit *log;
protected:
    void closeEvent(QCloseEvent *event) override;
};
