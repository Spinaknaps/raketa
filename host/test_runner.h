#pragma once

#include "test_scenario.h"

#include <functional>

class DeviceClient;

bool isWithinTolerance(const PressureStep &step, std::uint32_t measuredPa);

RunResult runPressureTest(
    DeviceClient &client,
    std::uint8_t mode,
    const std::array<PressureStep, 3> &scenario,
    const std::function<void(const StepResult &)> &onStep = {}
);