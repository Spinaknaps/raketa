#include "scenario_loader.h"

#include <tcl.h>

#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: tcl_check <scenario.tcl>\n";
        return 2;
    }

    Tcl_FindExecutable(argv[0]);

    LoadedScenario scenario;
    std::string error;
    const bool loaded = loadScenario(argv[1], scenario, error);
    Tcl_Finalize();

    if (!loaded) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << "Scenario: " << scenario.name << '\n';
    std::cout << "Steps: " << scenario.steps.size() << '\n';

    for (const auto &step : scenario.steps) {
        std::cout << step.stimulusPa << ' '
                  << step.expectedPa << ' '
                  << step.tolerancePa << '\n';
    }

    return 0;
}