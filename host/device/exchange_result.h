#pragma once

#include "exchange_status.h"
#include <cstdint>
#include <optional>

struct ExchangeResult {
    ExchangeStatus status = ExchangeStatus::ConnectionError;
    std::optional<std::uint32_t> pressurePa;
    std::optional<std::uint8_t> errorCode;
};

