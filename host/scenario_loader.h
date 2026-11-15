#pragma once

#include "test_scenario.h"

#include <string>
#include <vector>

struct LoadedScenario {
    std::string name;
    std::vector<PressureStep> steps;
};

bool loadScenario(
    const char *path,
    LoadedScenario &scenario,
    std::string &error
);