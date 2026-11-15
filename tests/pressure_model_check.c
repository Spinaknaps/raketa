#include "pressure_model.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    PressureModel model;
    uint32_t sample_pa;

    pressure_model_set_mode(&model, PRESSURE_MODE_NORMAL);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_NOT_SET);

    const uint32_t steps[] = {100000u, 90000u, 80000u};
    for (size_t i = 0; i < sizeof steps / sizeof steps[0]; ++i) {
        pressure_model_set_stimulus(&model, steps[i]);
        assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_OK);
        assert(sample_pa == steps[i]);
    }
    puts("normal OK");

    pressure_model_set_mode(&model, PRESSURE_MODE_STUCK);
    pressure_model_set_stimulus(&model, 100000u);
    pressure_model_set_stimulus(&model, 90000u);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_OK);
    assert(sample_pa == 100000u);

    pressure_model_set_mode(&model, PRESSURE_MODE_STUCK);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_NOT_SET);
    pressure_model_set_stimulus(&model, 80000u);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_OK);
    assert(sample_pa == 80000u);
    puts("stuck and reset OK");

    pressure_model_set_mode(&model, PRESSURE_MODE_SILENT);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_NOT_SET);
    pressure_model_set_stimulus(&model, 100000u);
    assert(pressure_model_read(&model, &sample_pa) == PRESSURE_SAMPLE_SILENT);
    puts("silent OK");

    return 0;
}