#include "command_handler.h"

enum {
    CMD_SET_MODE = 0x01,
    CMD_SET_STIMULUS = 0x02,
    CMD_READ_SAMPLE = 0x03,
    REPLY_SET_MODE = 0x81,
    REPLY_SET_STIMULUS = 0x82,
    REPLY_SAMPLE = 0x83,
    REPLY_ERROR = 0xFF
};

enum {
    ERROR_UNKNOWN_COMMAND = 0x01,
    ERROR_BAD_LENGTH = 0x02,
    ERROR_BAD_VALUE = 0x03,
    ERROR_NO_STIMULUS = 0x04
};

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static bool reply_error(ProtocolFrame *response, uint8_t error)
{
    response->type = REPLY_ERROR;
    response->payload_length = 1u;
    response->payload[0] = error;
    return true;
}

bool command_handle(
    PressureModel *model,
    const ProtocolFrame *request,
    ProtocolFrame *response
)
{
    response->request_id = request->request_id;
    response->payload_length = 0u;

    switch (request->type) {
    case CMD_SET_MODE: {
        if (request->payload_length != 1u) {
            return reply_error(response, ERROR_BAD_LENGTH);
        }

        const PressureMode modes[] = {
            PRESSURE_MODE_NORMAL,
            PRESSURE_MODE_STUCK,
            PRESSURE_MODE_SILENT
        };
        const uint8_t mode = request->payload[0];
        if (mode >= sizeof modes / sizeof modes[0]) {
            return reply_error(response, ERROR_BAD_VALUE);
        }

        pressure_model_set_mode(model, modes[mode]);
        response->type = REPLY_SET_MODE;
        return true;
    }

    case CMD_SET_STIMULUS: {
        if (request->payload_length != 4u) {
            return reply_error(response, ERROR_BAD_LENGTH);
        }

        const uint32_t pressure_pa = read_u32_le(request->payload);
        if (pressure_pa > 200000u) {
            return reply_error(response, ERROR_BAD_VALUE);
        }

        pressure_model_set_stimulus(model, pressure_pa);
        response->type = REPLY_SET_STIMULUS;
        return true;
    }

    case CMD_READ_SAMPLE: {
        if (request->payload_length != 0u) {
            return reply_error(response, ERROR_BAD_LENGTH);
        }

        uint32_t pressure_pa;
        const PressureSampleResult result =
            pressure_model_read(model, &pressure_pa);

        if (result == PRESSURE_SAMPLE_NOT_SET) {
            return reply_error(response, ERROR_NO_STIMULUS);
        }

        if (result == PRESSURE_SAMPLE_SILENT) {
            return false;
        }

        response->type = REPLY_SAMPLE;
        response->payload_length = 4u;
        write_u32_le(response->payload, pressure_pa);
        return true;
    }

    default:
        return reply_error(response, ERROR_UNKNOWN_COMMAND);
    }
}