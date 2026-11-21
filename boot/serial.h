#ifndef SIGIL_OS_SERIAL_H
#define SIGIL_OS_SERIAL_H

#include <stdint.h>

static inline void port_write(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t port_read(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void serial_init(void)
{
    port_write(0x3f9, 0);
    port_write(0x3fb, 0x80);
    port_write(0x3f8, 1);
    port_write(0x3f9, 0);
    port_write(0x3fb, 3);
    port_write(0x3fa, 7);
    port_write(0x3fc, 3);
}

static inline void serial_write(const char *message)
{
    for (; *message; ++message) {
        for (unsigned int tries = 0; tries < 100000; ++tries) {
            if (port_read(0x3fd) & 0x20) {
                port_write(0x3f8, (uint8_t)*message);
                break;
            }
        }
    }
}

static inline __attribute__((noreturn)) void boot_stop(const char *message)
{
    __asm__ volatile("cli" : : : "memory");
    serial_write(message);
    for (;;)
        __asm__ volatile("hlt");
}

#endif
