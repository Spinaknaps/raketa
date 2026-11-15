#include "../firmware/protocol.h"

#include <stdio.h>
#include <string.h>

static void print_bytes(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        printf("%02X ", (unsigned int)data[i]);
    }
    puts("");
}

int main(void)
{
    const uint8_t crc_input[] = "123456789";
    const uint16_t crc = protocol_crc16(crc_input, sizeof crc_input - 1u);

    printf("CRC check: %04X\n", (unsigned int)crc);
    if (crc != 0x29B1u) {
        return 1;
    }

    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];

    const uint8_t expected_read[] = {
        0xA5, 0x5A, 0x01, 0x03, 0x02, 0x00, 0x00, 0x00, 0x0A, 0x48
    };

    size_t length = protocol_encode_frame(
        frame, sizeof frame, 0x03u, 2u, NULL, 0u
    );

    printf("READ_SAMPLE: ");
    print_bytes(frame, length);

    if (length != sizeof expected_read ||
        memcmp(frame, expected_read, sizeof expected_read) != 0) {
        return 1;
        }

    const uint8_t pressure[] = {0xA0, 0x86, 0x01, 0x00};
    const uint8_t expected_set[] = {
        0xA5, 0x5A, 0x01, 0x02, 0x01, 0x00, 0x04, 0x00,
        0xA0, 0x86, 0x01, 0x00, 0xD4, 0x59
    };

    length = protocol_encode_frame(
        frame, sizeof frame, 0x02u, 1u, pressure, sizeof pressure
    );

    printf("SET_STIMULUS: ");
    print_bytes(frame, length);

    if (length != sizeof expected_set ||
        memcmp(frame, expected_set, sizeof expected_set) != 0) {
        return 1;
        }

    const uint8_t expected_set_ack[] = {
        0xA5, 0x5A, 0x01, 0x82, 0x01, 0x00, 0x00, 0x00, 0x57, 0x5B
    };

    length = protocol_encode_frame(
        frame, sizeof frame, 0x82u, 1u, NULL, 0u
    );

    printf("SET_STIMULUS ACK: ");
    print_bytes(frame, length);

    if (length != sizeof expected_set_ack ||
        memcmp(frame, expected_set_ack, sizeof expected_set_ack) != 0) {
        return 1;
        }

    const uint8_t expected_sample[] = {
        0xA5, 0x5A, 0x01, 0x83, 0x02, 0x00, 0x04, 0x00,
        0xA0, 0x86, 0x01, 0x00, 0x30, 0xC5
    };

    length = protocol_encode_frame(
        frame, sizeof frame, 0x83u, 2u, pressure, sizeof pressure
    );

    printf("READ_SAMPLE response: ");
    print_bytes(frame, length);

    if (length != sizeof expected_sample ||
        memcmp(frame, expected_sample, sizeof expected_sample) != 0) {
        return 1;
        }

    ProtocolDecoder decoder;
    ProtocolFrame decoded;
    protocol_decoder_init(&decoder);

    for (size_t i = 0; i < sizeof expected_read; ++i) {
        protocol_decoder_push(&decoder, expected_read[i], 0u);
        const bool ready = protocol_decoder_take(&decoder, 0u, &decoded);
        const bool last_byte = i + 1u == sizeof expected_read;

        if (ready != last_byte) {
            printf("Unexpected decoder result at byte %u\n", (unsigned int)i);
            return 1;
        }
    }

    if (decoded.type != 0x03u ||
        decoded.request_id != 2u ||
        decoded.payload_length != 0u) {
        puts("Decoded READ_SAMPLE fields differ");
        return 1;
        }

    puts("Decoder READ_SAMPLE OK");

    protocol_decoder_init(&decoder);

    for (size_t i = 0; i < 8u; ++i) {
        protocol_decoder_push(&decoder, expected_read[i], 0u);
        if (protocol_decoder_take(&decoder, 0u, &decoded)) {
            puts("Incomplete frame was accepted");
            return 1;
        }
    }

    if (protocol_decoder_take(&decoder, 999u, &decoded) ||
        decoder.used != 8u ||
        protocol_decoder_take(&decoder, 1000u, &decoded) ||
        decoder.used != 0u) {
        puts("Incomplete frame timeout failed");
        return 1;
        }
    puts("Incomplete frame timeout OK");

    protocol_decoder_init(&decoder);

    uint8_t damaged[sizeof expected_read];
    memcpy(damaged, expected_read, sizeof damaged);
    damaged[6] = 0x0Au;

    size_t decoded_count = 0u;
    for (size_t i = 0; i < sizeof damaged + sizeof expected_read; ++i) {
        const uint8_t byte = i < sizeof damaged
            ? damaged[i]
            : expected_read[i - sizeof damaged];

        protocol_decoder_push(&decoder, byte, 0u);
        while (protocol_decoder_take(&decoder, 0u, &decoded)) {
            if (i + 1u != sizeof damaged + sizeof expected_read ||
                decoded.type != 0x03u ||
                decoded.request_id != 2u ||
                decoded.payload_length != 0u) {
                puts("Unexpected frame during recovery");
                return 1;
                }
            ++decoded_count;
        }
    }

    if (decoded_count != 1u || decoder.used != 0u) {
        puts("Recovery after bad length failed");
        return 1;
    }
    puts("Recovery after bad length OK");
    
    puts("Protocol checks OK");
    return 0;
}