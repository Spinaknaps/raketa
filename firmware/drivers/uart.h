#ifndef RAKETA_UART_H
#define RAKETA_UART_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void uart0_init(void);
void uart0_write_bytes(const uint8_t *data, size_t length);
bool uart0_try_read_byte(uint8_t *byte);

#endif