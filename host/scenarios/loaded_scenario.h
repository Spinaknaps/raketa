#pragma once

#include "pressure_step.h"
#include <string>
#include <vector>

struct LoadedScenario {
    std::string name;
    std::vector<PressureStep> steps;
};

