#include "pressure_chart.h"

#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <algorithm>

namespace {

struct Plot {
    QRectF bounds;
    std::size_t stepCount;
    std::uint32_t upperPa;

    [[nodiscard]] qreal xForStep(std::size_t number) const
    {
        return bounds.left() + bounds.width() *
            (static_cast<qreal>(number) - 0.5) / static_cast<qreal>(stepCount);
    }

    [[nodiscard]] QPointF position(std::size_t number, std::uint32_t pressure) const
    {
        return {xForStep(number), bounds.bottom() -
            bounds.height() * static_cast<qreal>(pressure) / static_cast<qreal>(upperPa)};
    }
};

std::uint32_t upperPressure(const std::vector<StepResult> &steps)
{
    std::uint32_t upperPa = 100000u;
    for (const auto &step : steps) {
        upperPa = std::max({upperPa, step.expectedPa, step.measuredPa.value_or(0u)});
    }
    return upperPa;
}

void drawAxes(QPainter &painter, const Plot &plot, const QColor &color)
{
    const auto &bounds = plot.bounds;
    painter.setPen(color);
    painter.drawLine(bounds.topLeft(), bounds.bottomLeft());
    painter.drawLine(bounds.bottomLeft(), bounds.bottomRight());
    painter.drawText(4, 32, QStringLiteral("%1 Па").arg(plot.upperPa));
    painter.drawText(4, static_cast<int>(bounds.bottom()), QStringLiteral("0"));

    for (std::size_t number = 1; number <= plot.stepCount; ++number) {
        painter.drawText(
            QPointF(plot.xForStep(number) - 4, bounds.bottom() + 18),
            QString::number(static_cast<qulonglong>(number))
        );
    }
}

void drawSeries(QPainter &painter, const Plot &plot, const std::vector<StepResult> &steps,
                const QColor &color, bool measured)
{
    painter.setPen(QPen(color, 2));
    painter.setBrush(color);

    std::optional<QPointF> previous;
    for (const auto &step : steps) {
        const auto value = measured ? step.measuredPa : std::optional{step.expectedPa};
        if (!value) {
            previous.reset();
            continue;
        }

        const auto point = plot.position(step.stepNumber, *value);
        if (previous) {
            painter.drawLine(*previous, point);
        }
        painter.drawEllipse(point, 3, 3);
        previous = point;
    }
}

} // namespace

PressureChart::PressureChart(QWidget *parent)
    : QWidget(parent) {
    setMinimumSize(320, 150);
}

void PressureChart::reset(std::size_t stepCount) {
    stepCount_ = stepCount;
    steps_.clear();
    update();
}

void PressureChart::appendStep(const StepResult &step) {
    steps_.push_back(step);
    update();
}

void PressureChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor expectedColor("#3486c9");
    const QColor measuredColor("#d17a20");
    painter.setPen(expectedColor);
    painter.drawText(65, 18, QStringLiteral("Эталон"));
    painter.setPen(measuredColor);
    painter.drawText(145, 18, QStringLiteral("Измерение"));

    const Plot plot{{65, 28, static_cast<qreal>(width() - 85),
                    static_cast<qreal>(height() - 58)},
                    stepCount_, upperPressure(steps_)};
    drawAxes(painter, plot, palette().color(QPalette::Text));
    if (stepCount_ != 0u) {
        drawSeries(painter, plot, steps_, expectedColor, false);
        drawSeries(painter, plot, steps_, measuredColor, true);
    }
}
