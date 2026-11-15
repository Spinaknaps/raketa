#include "command_handler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_error(
    PressureModel *model, const ProtocolFrame *request, uint8_t code
)
{
    ProtocolFrame response;
    assert(command_handle(model, request, &response));
    assert(response.request_id == request->request_id);
    assert(response.type == 0xFFu);
    assert(response.payload_length == 1u);
    assert(response.payload[0] == code);
}

int main(void)
{
    PressureModel model;
    ProtocolFrame response;
    pressure_model_set_mode(&model, PRESSURE_MODE_NORMAL);

    ProtocolFrame read = {
        .type = 0x03u, .request_id = 12u, .payload_length = 0u
    };
    expect_error(&model, &read, 0x04u);

    ProtocolFrame set = {
        .type = 0x02u, .request_id = 11u, .payload_length = 4u,
        .payload = {0xA0u, 0x86u, 0x01u, 0x00u}
    };
    assert(command_handle(&model, &set, &response));
    assert(response.type == 0x82u);
    assert(response.request_id == 11u);
    assert(response.payload_length == 0u);

    assert(command_handle(&model, &read, &response));
    assert(response.type == 0x83u);
    assert(response.request_id == 12u);
    assert(response.payload_length == 4u);
    assert(memcmp(response.payload, set.payload, 4u) == 0);
    puts("normal exchange OK");

    ProtocolFrame invalid = set;
    invalid.payload_length = 3u;
    expect_error(&model, &invalid, 0x02u);

    invalid = set;
    invalid.payload[3] = 1u; /* Делает давление больше 200000 Па. */
    expect_error(&model, &invalid, 0x03u);

    invalid.type = 0x55u;
    expect_error(&model, &invalid, 0x01u);

    assert(command_handle(&model, &read, &response));
    assert(response.type == 0x83u);
    assert(response.payload_length == 4u);
    assert(memcmp(response.payload, set.payload, 4u) == 0);
    puts("invalid requests preserve reading OK");

    ProtocolFrame mode = {
        .type = 0x01u, .request_id = 13u, .payload_length = 1u,
        .payload = {2u}
    };
    assert(command_handle(&model, &mode, &response));
    assert(response.type == 0x81u);
    assert(response.request_id == 13u);
    assert(response.payload_length == 0u);

    expect_error(&model, &read, 0x04u);
    assert(command_handle(&model, &set, &response));
    assert(response.type == 0x82u);
    assert(!command_handle(&model, &read, &response));
    puts("silent exchange OK");

    puts("Command checks OK");
    return 0;
}