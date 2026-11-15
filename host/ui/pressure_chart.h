#pragma once
#include "step_result.h"
#include <QWidget>
#include <vector>

class PressureChart : public QWidget {
public:
    explicit PressureChart(QWidget *parent = nullptr);
    void reset(std::size_t stepCount);
    void appendStep(const StepResult &step);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    std::vector<StepResult> steps_;
    std::size_t stepCount_ = 0u;
};
