#ifndef RAKETA_PROTOCOL_H
#define RAKETA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_MAX_PAYLOAD_SIZE 32u
#define PROTOCOL_MAX_FRAME_SIZE   42u

uint16_t protocol_crc16(const uint8_t *data, size_t length);

size_t protocol_encode_frame(
    uint8_t *output,
    size_t output_capacity,
    uint8_t type,
    uint16_t request_id,
    const uint8_t *payload,
    size_t payload_length
);

typedef struct {
    uint8_t type;
    uint16_t request_id;
    size_t payload_length;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
} ProtocolFrame;

typedef struct {
    uint8_t bytes[PROTOCOL_MAX_FRAME_SIZE];
    uint32_t received_ms[PROTOCOL_MAX_FRAME_SIZE];
    size_t used;
} ProtocolDecoder;

void protocol_decoder_init(ProtocolDecoder *decoder);
bool protocol_decoder_take(
    ProtocolDecoder *decoder, uint32_t now_ms, ProtocolFrame *frame
);
void protocol_decoder_push(
    ProtocolDecoder *decoder, uint8_t byte, uint32_t now_ms
);

#endif