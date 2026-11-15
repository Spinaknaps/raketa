#include "protocol.h"

uint16_t protocol_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);

        for (unsigned int bit = 0; bit < 8u; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

static void write_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)(value >> 8);
}

size_t protocol_encode_frame(
    uint8_t *output,
    size_t output_capacity,
    uint8_t type,
    uint16_t request_id,
    const uint8_t *payload,
    size_t payload_length
)
{
    if (output == NULL || payload_length > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return 0;
    }

    if (payload_length != 0u && payload == NULL) {
        return 0;
    }

    const size_t frame_size = 10u + payload_length;
    if (output_capacity < frame_size) {
        return 0;
    }

    output[0] = 0xA5u;
    output[1] = 0x5Au;
    output[2] = 0x01u;
    output[3] = type;
    write_u16_le(&output[4], request_id);
    write_u16_le(&output[6], (uint16_t)payload_length);

    for (size_t i = 0; i < payload_length; ++i) {
        output[8u + i] = payload[i];
    }

    const uint16_t crc = protocol_crc16(&output[2], 6u + payload_length);
    write_u16_le(&output[8u + payload_length], crc);

    return frame_size;


}

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void discard_prefix(ProtocolDecoder *decoder, size_t count)
{
    decoder->used -= count;

    for (size_t i = 0; i < decoder->used; ++i) {
        decoder->bytes[i] = decoder->bytes[i + count];
        decoder->received_ms[i] = decoder->received_ms[i + count];
    }
}

void protocol_decoder_init(ProtocolDecoder *decoder)
{
    decoder->used = 0u;
}

void protocol_decoder_push(
    ProtocolDecoder *decoder, uint8_t byte, uint32_t now_ms
)
{
    if (decoder->used == PROTOCOL_MAX_FRAME_SIZE) {
        discard_prefix(decoder, 1u);
    }

    decoder->bytes[decoder->used] = byte;
    decoder->received_ms[decoder->used] = now_ms;
    ++decoder->used;
}

bool protocol_decoder_take(
    ProtocolDecoder *decoder, uint32_t now_ms, ProtocolFrame *frame
)
{
    const uint8_t *bytes = decoder->bytes;

    while (decoder->used != 0u) {
        if (bytes[0] != 0xA5u ||
            (decoder->used >= 2u && bytes[1] != 0x5Au)) {
            discard_prefix(decoder, 1u);
            continue;
        }

        if (decoder->used < 2u) {
            return false;
        }

        if ((uint32_t)(now_ms - decoder->received_ms[1]) >= 1000u) {
            discard_prefix(decoder, 1u);
            continue;
        }

        if (decoder->used < 8u) {
            return false;
        }

        const size_t payload_length = read_u16_le(&bytes[6]);
        if (bytes[2] != 0x01u || payload_length > PROTOCOL_MAX_PAYLOAD_SIZE) {
            discard_prefix(decoder, 1u);
            continue;
        }

        const size_t frame_size = 10u + payload_length;
        if (decoder->used < frame_size) {
            return false;
        }

        const uint16_t received_crc = read_u16_le(&bytes[8u + payload_length]);
        const uint16_t calculated_crc =
            protocol_crc16(&bytes[2], 6u + payload_length);

        if (received_crc != calculated_crc) {
            discard_prefix(decoder, 1u);
            continue;
        }

        frame->type = bytes[3];
        frame->request_id = read_u16_le(&bytes[4]);
        frame->payload_length = payload_length;
        for (size_t i = 0; i < payload_length; ++i) {
            frame->payload[i] = bytes[8u + i];
        }

        discard_prefix(decoder, frame_size);
        return true;
    }

    return false;
}