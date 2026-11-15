#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "test_outcome.h"

struct StepResult {
    std::size_t stepNumber;
    std::uint32_t expectedPa;
    std::uint32_t tolerancePa;
    std::optional<std::uint32_t> measuredPa;
    TestOutcome outcome;
    std::string detail;
};
