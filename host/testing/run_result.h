#pragma once

#include "step_result.h"
#include <vector>

struct RunResult {
    TestOutcome outcome;
    std::vector<StepResult> steps;
    std::string detail;
};
