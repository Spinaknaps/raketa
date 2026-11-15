#pragma once

#include <cstdint>

struct PressureStep {
    std::uint32_t stimulusPa;
    std::uint32_t expectedPa;
    std::uint32_t tolerancePa;
};
