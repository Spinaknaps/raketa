#include "pressure_model.h"

void pressure_model_set_mode(PressureModel *model, PressureMode mode)
{
    model->mode = mode;
    model->has_stimulus = false;
    model->reading_pa = 0u;
}

void pressure_model_set_stimulus(PressureModel *model, uint32_t pressure_pa)
{
    if (model->mode != PRESSURE_MODE_STUCK || !model->has_stimulus) {
        model->reading_pa = pressure_pa;
    }

    model->has_stimulus = true;
}

PressureSampleResult pressure_model_read(
    const PressureModel *model, uint32_t *pressure_pa
)
{
    if (!model->has_stimulus) {
        return PRESSURE_SAMPLE_NOT_SET;
    }

    if (model->mode == PRESSURE_MODE_SILENT) {
        return PRESSURE_SAMPLE_SILENT;
    }

    *pressure_pa = model->reading_pa;
    return PRESSURE_SAMPLE_OK;
}