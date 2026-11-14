#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define UART0_DR    REG32(0x4000C000u)  /* Регистр данных */
#define UART0_FR    REG32(0x4000C018u)  /* Флаги состояния */
#define UART0_LCRH  REG32(0x4000C02Cu)  /* Формат слова */
#define UART0_CTL   REG32(0x4000C030u)  /* Управление UART */

#define UART_TX_FIFO_FULL (1u << 5)
#define UART_WORD_LENGTH_8 (3u << 5)
#define UART_ENABLE (1u << 0)
#define UART_TX_ENABLE (1u << 8)
#define UART_RX_FIFO_EMPTY (1u << 4)
#define UART_RX_ENABLE     (1u << 9)

static void uart0_init(void)
{
    UART0_CTL = 0;
    UART0_LCRH = UART_WORD_LENGTH_8;
    UART0_CTL = UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE;
}

static void uart0_write(const char *message)
{
    while (*message != '\0') {
        while ((UART0_FR & UART_TX_FIFO_FULL) != 0u) {
            /* Ждём места для следующего байта. */
        }

        UART0_DR = (uint8_t)*message;
        ++message;
    }
}

static uint8_t uart0_read_byte(void)
{
    while ((UART0_FR & UART_RX_FIFO_EMPTY) != 0u) {
        /* Ждём, пока UART получит байт. */
    }

    return (uint8_t)UART0_DR;
}

int main(void)
{
    char command[16];
    unsigned int length = 0;
    int too_long = 0;

    uart0_init();
    uart0_write("READY\r\n");

    for (;;) {
        uint8_t byte = uart0_read_byte();

        if (byte == '\r' || byte == '\n') {
            if (too_long) {
                uart0_write("ERR line too long\r\n");
            } else if (length == 4 &&
                       command[0] == 'P' && command[1] == 'I' &&
                       command[2] == 'N' && command[3] == 'G') {
                uart0_write("PONG\r\n");
                       } else if (length != 0) {
                           uart0_write("ERR unknown command\r\n");
                       }

            length = 0;
            too_long = 0;
        } else if (length < sizeof command) {
            command[length] = (char)byte;
            ++length;
        } else {
            too_long = 1;
        }
    }
}