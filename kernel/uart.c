#include "uart.h"
#include "serial.h"

#define UART_RBR 0
#define UART_THR 0
#define UART_IER 1
#define UART_IIR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5

#define LSR_DATA_READY 0x01
#define LSR_ERRORS     0x0e /* オーバーラン・パリティ・フレーミング */
#define LSR_THR_EMPTY  0x20
#define MCR_LOOPBACK   0x10
#define MCR_OUT_BITS   0x0f

#define UART_WAIT_TRIES 100000u

static uint8_t reg_read(uint16_t base, uint16_t off)
{
    return port_read((uint16_t)(base + off));
}

static void reg_write(uint16_t base, uint16_t off, uint8_t value)
{
    port_write((uint16_t)(base + off), value);
}

static void drain(uint16_t base)
{
    for (unsigned int i = 0; i < 64 && (reg_read(base, UART_LSR) & LSR_DATA_READY); ++i)
        (void)reg_read(base, UART_RBR);
}

int uart16550_read_nowait(uint16_t base, uint8_t *rx)
{
    if (!rx)
        return -1;
    if (!(reg_read(base, UART_LSR) & LSR_DATA_READY))
        return -1;
    *rx = reg_read(base, UART_RBR);
    return 0;
}

int uart16550_fifo_enabled(uint16_t base)
{
    return (reg_read(base, UART_IIR) & 0xc0) == 0xc0;
}

int uart16550_loopback_byte(uint16_t base, uint8_t tx, uint8_t *rx)
{
    if (!rx)
        return -1;

    uint8_t saved_ier = reg_read(base, UART_IER);
    uint8_t saved_mcr = reg_read(base, UART_MCR);
    int result = -1;

    /* 割り込みを止め、ループバックへ入る。 */
    reg_write(base, UART_IER, 0);
    reg_write(base, UART_MCR, (uint8_t)(MCR_LOOPBACK | MCR_OUT_BITS));
    drain(base);

    unsigned int tries = 0;
    while (!(reg_read(base, UART_LSR) & LSR_THR_EMPTY)) {
        if (++tries >= UART_WAIT_TRIES)
            goto restore;
    }
    reg_write(base, UART_THR, tx);

    tries = 0;
    for (;;) {
        uint8_t lsr = reg_read(base, UART_LSR);
        if (lsr & LSR_DATA_READY) {
            if (lsr & LSR_ERRORS)
                goto restore;
            *rx = reg_read(base, UART_RBR);
            result = 0;
            break;
        }
        if (++tries >= UART_WAIT_TRIES)
            goto restore;
    }

restore:
    drain(base);
    reg_write(base, UART_MCR, saved_mcr);
    reg_write(base, UART_IER, saved_ier);
    return result;
}
