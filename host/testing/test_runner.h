#pragma once

#include "pressure_step.h"
#include "run_result.h"

#include <functional>

class DeviceClient;

bool isWithinTolerance(const PressureStep &step, std::uint32_t measuredPa);

RunResult runPressureTest(
    DeviceClient &client,
    std::uint8_t mode,
    const std::vector<PressureStep> &scenario,
    const std::function<void(const StepResult &)> &onStep = {}
);