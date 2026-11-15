#include "command_handler.h"
#include "uart.h"
#include "timer.h"

int main(void)
{
    PressureModel model;
    ProtocolDecoder decoder;
    ProtocolFrame request;
    ProtocolFrame response;
    uint8_t output[PROTOCOL_MAX_FRAME_SIZE];

    uart0_init();
    pressure_model_set_mode(&model, PRESSURE_MODE_NORMAL);
    protocol_decoder_init(&decoder);
    timer_init();

    while (true) {
        const uint32_t now_ms = timer_poll_ms();

        uint8_t byte;
        if (uart0_try_read_byte(&byte)) {
            protocol_decoder_push(&decoder, byte, now_ms);
        }

        while (protocol_decoder_take(&decoder, now_ms, &request)) {
            if (!command_handle(&model, &request, &response)) {
                continue;
            }

            const size_t length = protocol_encode_frame(
                output, sizeof output, response.type, response.request_id,
                response.payload, response.payload_length
            );

            if (length != 0u) {
                uart0_write_bytes(output, length);
            }
        }
    }
}