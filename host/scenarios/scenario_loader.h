#pragma once

#include "loaded_scenario.h"

bool loadScenario(
    const char *path,
    LoadedScenario &scenario,
    std::string &error
);