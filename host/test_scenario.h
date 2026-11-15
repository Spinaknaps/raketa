#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PressureStep {
    std::uint32_t stimulusPa;
    std::uint32_t expectedPa;
    std::uint32_t tolerancePa;
};

inline constexpr std::array<PressureStep, 3> pressureScenario{{
    {100000u, 100000u, 1000u},
    { 90000u,  90000u, 1000u},
    { 80000u,  80000u, 1000u}
}};

enum class TestOutcome {
    Pass,
    Fail,
    Timeout,
    CommandError,
    ConnectionError,
    Cancelled
};

struct StepResult {
    std::size_t stepNumber;
    std::uint32_t expectedPa;
    std::uint32_t tolerancePa;
    std::optional<std::uint32_t> measuredPa;
    TestOutcome outcome;
    std::string detail;
};

struct RunResult {
    TestOutcome outcome;
    std::vector<StepResult> steps;
    std::string detail;
};