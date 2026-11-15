#include "scenario_loader.h"

#include <tcl.h>

#include <memory>
#include <utility>

namespace {

int scriptError(Tcl_Interp *interp, const char *message)
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj(message, -1));
    return TCL_ERROR;
}

int defineScenario(
    ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]
)
{
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "name");
        return TCL_ERROR;
    }

    auto &scenario = *static_cast<LoadedScenario *>(data);
    if (!scenario.name.empty()) {
        return scriptError(interp, "scenario name is already defined");
    }

    const std::string name = Tcl_GetString(objv[1]);
    if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
        return scriptError(interp, "scenario name must not be empty");
    }

    scenario.name = name;
    return TCL_OK;
}

int addStep(
    ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]
)
{
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "stimulusPa expectedPa tolerancePa");
        return TCL_ERROR;
    }

    Tcl_WideInt values[3];
    for (int i = 0; i < 3; ++i) {
        if (Tcl_GetWideIntFromObj(interp, objv[i + 1], &values[i]) != TCL_OK) {
            return TCL_ERROR;
        }
        if (values[i] < 0 || values[i] > 200000) {
            return scriptError(
                interp, "step values must be between 0 and 200000 Pa"
            );
        }
    }

    auto &scenario = *static_cast<LoadedScenario *>(data);
    scenario.steps.push_back({
        static_cast<std::uint32_t>(values[0]),
        static_cast<std::uint32_t>(values[1]),
        static_cast<std::uint32_t>(values[2])
    });
    return TCL_OK;
}

} // namespace

bool loadScenario(
    const char *path,
    LoadedScenario &scenario,
    std::string &error
)
{
    LoadedScenario loaded;
    std::unique_ptr<Tcl_Interp, decltype(&Tcl_DeleteInterp)> interp(
        Tcl_CreateInterp(), Tcl_DeleteInterp
    );

    error.clear();

    if (Tcl_Init(interp.get()) != TCL_OK ||
        Tcl_MakeSafe(interp.get()) != TCL_OK) {
        error = std::string("Tcl initialization: ") +
                Tcl_GetStringResult(interp.get());
        return false;
    }

    Tcl_CreateObjCommand(
        interp.get(), "scenario", defineScenario, &loaded, nullptr
    );
    Tcl_CreateObjCommand(
        interp.get(), "step", addStep, &loaded, nullptr
    );

    Tcl_Obj *file = Tcl_NewStringObj(path, -1);
    Tcl_IncrRefCount(file);
    const int status = Tcl_FSEvalFileEx(interp.get(), file, "utf-8");
    Tcl_DecrRefCount(file);

    if (status != TCL_OK) {
        error = std::string(path) + ":" +
                std::to_string(Tcl_GetErrorLine(interp.get())) + ": " +
                Tcl_GetStringResult(interp.get());
        return false;
    }

    if (loaded.name.empty() || loaded.steps.empty()) {
        error = std::string(path) +
                ": expected a scenario name and at least one step";
        return false;
    }

    scenario = std::move(loaded);
    return true;
}