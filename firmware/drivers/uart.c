#include "uart.h"

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define UART0_DR    REG32(0x4000C000u)  /* Регистр данных */
#define UART0_FR    REG32(0x4000C018u)  /* Флаги состояния */
#define UART0_LCRH  REG32(0x4000C02Cu)  /* Формат слова */
#define UART0_CTL   REG32(0x4000C030u)  /* Управление UART */

#define UART_WORD_LENGTH_8 (3u << 5)
#define UART_TX_FIFO_FULL (1u << 5)
#define UART_ENABLE (1u << 0)
#define UART_TX_ENABLE (1u << 8)
#define UART_RX_FIFO_EMPTY (1u << 4)
#define UART_RX_ENABLE     (1u << 9)

void uart0_init(void)
{
    UART0_CTL = 0;
    UART0_LCRH = UART_WORD_LENGTH_8;
    UART0_CTL = UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE;
}

void uart0_write_bytes(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        while ((UART0_FR & UART_TX_FIFO_FULL) != 0u) {
            /* Ждём места для следующего байта. */
        }

        UART0_DR = data[i];
    }
}

bool uart0_try_read_byte(uint8_t *byte)
{
    if ((UART0_FR & UART_RX_FIFO_EMPTY) != 0u) {
        return false;
    }

    *byte = (uint8_t)UART0_DR;
    return true;
}
