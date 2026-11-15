#include "test_runner.h"
#include "device_client.h"

bool isWithinTolerance(const PressureStep &step, std::uint32_t measuredPa)
{
    const auto difference = measuredPa >= step.expectedPa
        ? measuredPa - step.expectedPa
        : step.expectedPa - measuredPa;

    return difference <= step.tolerancePa;
}

static TestOutcome exchangeOutcome(ExchangeStatus status)
{
    switch (status) {
        case ExchangeStatus::Ok:
            return TestOutcome::Pass;
        case ExchangeStatus::Timeout:
            return TestOutcome::Timeout;
        case ExchangeStatus::CommandError:
            return TestOutcome::CommandError;
        case ExchangeStatus::ConnectionError:
            return TestOutcome::ConnectionError;
        case ExchangeStatus::Cancelled:
            return TestOutcome::Cancelled;
    }

    return TestOutcome::ConnectionError;
}

static std::string exchangeDetail(
    const char *command, const ExchangeResult &result
)
{
    const std::string prefix = std::string(command) + ": ";

    switch (result.status) {
        case ExchangeStatus::Ok:
            return {};
        case ExchangeStatus::Timeout:
            return prefix + "response deadline expired";
        case ExchangeStatus::ConnectionError:
            return prefix + "TCP connection error";
        case ExchangeStatus::CommandError:
            if (result.errorCode) {
                return prefix + "device error code " +
                       std::to_string(*result.errorCode);
            }
            return prefix + "device rejected command";
        case ExchangeStatus::Cancelled:
            return prefix + "cancelled";
    }

    return prefix + "unknown exchange status";
}

RunResult runPressureTest(
    DeviceClient &client,
    std::uint8_t mode,
    const std::array<PressureStep, 3> &scenario,
    const std::function<void(const StepResult &)> &onStep
)
{
    RunResult run{TestOutcome::Pass, {}, {}};

    const auto modeResult = client.setMode(mode);
    if (modeResult.status != ExchangeStatus::Ok) {
        run.outcome = exchangeOutcome(modeResult.status);
        run.detail = exchangeDetail("SET_MODE", modeResult);
        return run;
    }

    for (std::size_t i = 0; i < scenario.size(); ++i) {
        const auto &step = scenario[i];
        auto &result = run.steps.emplace_back(StepResult{
            i + 1u, step.expectedPa, step.tolerancePa,
            std::nullopt, TestOutcome::Pass, {}
        });

        const auto stimulus = client.setStimulus(step.stimulusPa);
        if (stimulus.status != ExchangeStatus::Ok) {
            result.outcome = exchangeOutcome(stimulus.status);
            result.detail = exchangeDetail("SET_STIMULUS", stimulus);
        } else {
            const auto sample = client.readSample();
            if (sample.status != ExchangeStatus::Ok) {
                result.outcome = exchangeOutcome(sample.status);
                result.detail = exchangeDetail("READ_SAMPLE", sample);
            } else if (!sample.pressurePa) {
                result.outcome = TestOutcome::CommandError;
                result.detail = "READ_SAMPLE: response has no pressure";
            } else {
                result.measuredPa = sample.pressurePa;

                if (!isWithinTolerance(step, *result.measuredPa)) {
                    result.outcome = TestOutcome::Fail;
                    result.detail = "Pressure outside allowed range";
                }
            }
        }

        if (onStep) {
            onStep(result);
        }

        if (result.outcome != TestOutcome::Pass) {
            run.outcome = result.outcome;
            run.detail = "Step " + std::to_string(result.stepNumber) +
                         ": " + result.detail;
            return run;
        }
    }

    return run;
}