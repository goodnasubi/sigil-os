#include "interrupt.h"
#include "serial.h"

static struct idt_gate idt[IDT_ENTRIES];
static struct idt_ptr idtr;

static volatile uint64_t interrupt_counter = 0;
static volatile uint64_t last_received_vector = 0;

extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);
extern void isr_stub_32(void);
extern void isr_stub_default(void);

typedef void (*isr_handler_t)(void);

static const isr_handler_t isr_stubs[33] = {
    isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,
    isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7,
    isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11,
    isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
    isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
    isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
    isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
    isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31,
    isr_stub_32
};

static inline uint16_t get_cs(void)
{
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void set_idt_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr)
{
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved = 0;
}

void interrupt_dispatch(struct isr_regs *regs)
{
    interrupt_counter++;
    last_received_vector = regs->vector;
}

void interrupt_init(void)
{
    uint16_t cs = get_cs();

    /* 0x8E = 10001110b : Present(1), DPL(00), 0, Interrupt Gate 64-bit(1110) */
    uint8_t gate_attr = 0x8E;

    /* Set up stubs for vectors 0..32 */
    for (uint32_t i = 0; i <= 32; ++i) {
        set_idt_gate((uint8_t)i, (uint64_t)(uintptr_t)isr_stubs[i], cs, gate_attr);
    }

    /* Set fallback for remaining vectors */
    for (uint32_t i = 33; i < IDT_ENTRIES; ++i) {
        set_idt_gate((uint8_t)i, (uint64_t)(uintptr_t)isr_stub_default, cs, gate_attr);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)(uintptr_t)&idt[0];

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

uint64_t interrupt_get_count(void)
{
    return interrupt_counter;
}

uint64_t interrupt_get_last_vector(void)
{
    return last_received_vector;
}

void test_interrupt_receive(void)
{
    interrupt_counter = 0;
    last_received_vector = 0;

    /* Issue software interrupt for vector 0x20 (32) */
    __asm__ volatile("int $0x20");

    if (interrupt_counter != 1 || last_received_vector != TEST_INTERRUPT_VECTOR) {
        boot_stop("STOP interrupt test: vector receipt failed\r\n");
    }

    serial_write("interrupt test received=OK vector=0x20 count=1\r\n");
}
