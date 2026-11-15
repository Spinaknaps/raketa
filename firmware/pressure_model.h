#ifndef RAKETA_PRESSURE_MODEL_H
#define RAKETA_PRESSURE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PRESSURE_MODE_NORMAL,
    PRESSURE_MODE_STUCK,
    PRESSURE_MODE_SILENT
} PressureMode;

typedef enum {
    PRESSURE_SAMPLE_OK,
    PRESSURE_SAMPLE_NOT_SET,
    PRESSURE_SAMPLE_SILENT
} PressureSampleResult;

typedef struct {
    PressureMode mode;
    bool has_stimulus;
    uint32_t reading_pa;
} PressureModel;

void pressure_model_set_mode(PressureModel *model, PressureMode mode);
void pressure_model_set_stimulus(PressureModel *model, uint32_t pressure_pa);
PressureSampleResult pressure_model_read(
    const PressureModel *model, uint32_t *pressure_pa
);

#endif