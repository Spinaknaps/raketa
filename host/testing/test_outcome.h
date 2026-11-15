#pragma once

enum class TestOutcome {
    Pass,
    Fail,
    Timeout,
    CommandError,
    ConnectionError,
    Cancelled
};

inline constexpr const char *outcomeName(TestOutcome outcome)
{
    switch (outcome) {
    case TestOutcome::Pass:            return "PASS";
    case TestOutcome::Fail:            return "FAIL";
    case TestOutcome::Timeout:         return "TIMEOUT";
    case TestOutcome::CommandError:    return "COMMAND_ERROR";
    case TestOutcome::ConnectionError: return "CONNECTION_ERROR";
    case TestOutcome::Cancelled:       return "CANCELLED";
    }
    return "UNKNOWN";
}
