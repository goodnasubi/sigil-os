#include "board_test.h"
#include "serial.h"
#include "page.h"
#include "interrupt.h"
#include "uart.h"

static void write_u64(uint64_t value)
{
    char buf[21];
    unsigned int n = 0;
    do {
        buf[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    char out[22];
    unsigned int i = 0;
    while (n)
        out[i++] = buf[--n];
    out[i] = 0;
    serial_write(out);
}

static void test_memory(void)
{
    uint64_t managed = page_total_managed();
    uint64_t free_pages = page_free_count();
    uint64_t allocated = page_allocated_count();
    if (managed == 0 || free_pages + allocated != managed)
        boot_stop("STOP p6-4 memory accounting\r\n");
    serial_write("p6-4 memory init: OK managed_pages=");
    write_u64(managed);
    serial_write(" free_pages=");
    write_u64(free_pages);
    serial_write("\r\n");
}

static void test_interrupt(void)
{
    uint64_t count = interrupt_get_count();
    if (count < 1 || interrupt_get_last_vector() != TEST_INTERRUPT_VECTOR)
        boot_stop("STOP p6-4 interrupt not observed\r\n");
    serial_write("p6-4 interrupt: OK vector=0x20 count=");
    write_u64(count);
    serial_write("\r\n");
}

static void test_uart(void)
{
    static const uint8_t pattern[] = {0x00, 0x55, 0xaa, 0xff, 'S', 'I', 'G', 'L'};
    unsigned int ok = 0;
    for (unsigned int i = 0; i < sizeof(pattern); ++i) {
        uint8_t rx = (uint8_t)~pattern[i];
        if (uart16550_loopback_byte(UART_COM1_BASE, pattern[i], &rx) != 0 || rx != pattern[i])
            boot_stop("STOP p6-4 uart loopback mismatch\r\n");
        ++ok;
    }
    serial_write("p6-4 uart loopback: OK com1=0x3f8 bytes=");
    write_u64(ok);
    serial_write(uart16550_fifo_enabled(UART_COM1_BASE) ? " fifo=on\r\n" : " fifo=off\r\n");

    /* 範囲外・不正な要求は拒否する。 */
    uint8_t rx = 0;
    if (uart16550_loopback_byte(UART_COM1_BASE, 0x41, 0) == 0 ||
        uart16550_read_nowait(UART_COM1_BASE, 0) == 0 ||
        uart16550_read_nowait(UART_COM1_BASE, &rx) == 0)
        boot_stop("STOP p6-4 uart invalid request not rejected\r\n");
    serial_write("p6-4 uart rejected: OK\r\n");
}

void run_p6_4_board_tests(void)
{
    test_memory();
    test_interrupt();
    test_uart();
    serial_write("p6-4 board observation: ALL CHECKS PASSED\r\n");
}
