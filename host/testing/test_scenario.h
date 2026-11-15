#pragma once

#include "pressure_step.h"
#include <vector>

inline const std::vector<PressureStep> pressureScenario{
    {100000u, 100000u, 1000u},
    { 90000u,  90000u, 1000u},
    { 80000u,  80000u, 1000u}
};
