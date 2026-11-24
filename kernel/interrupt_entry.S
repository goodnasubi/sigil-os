.text
.globl isr_common
.type isr_common, @function
isr_common:
    cld
    push %rax
    push %rcx
    push %rdx
    push %rbx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi
    call interrupt_dispatch

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rbx
    pop %rdx
    pop %rcx
    pop %rax

    add $16, %rsp
    iretq
.size isr_common, . - isr_common

.macro ISR_NOERR_STUB num
.globl isr_stub_\num
.type isr_stub_\num, @function
isr_stub_\num:
    push $0
    push $\num
    jmp isr_common
.size isr_stub_\num, . - isr_stub_\num
.endm

/* Stubs for vectors 0 to 32 */
ISR_NOERR_STUB 0
ISR_NOERR_STUB 1
ISR_NOERR_STUB 2
ISR_NOERR_STUB 3
ISR_NOERR_STUB 4
ISR_NOERR_STUB 5
ISR_NOERR_STUB 6
ISR_NOERR_STUB 7
ISR_NOERR_STUB 8
ISR_NOERR_STUB 9
ISR_NOERR_STUB 10
ISR_NOERR_STUB 11
ISR_NOERR_STUB 12
ISR_NOERR_STUB 13
ISR_NOERR_STUB 14
ISR_NOERR_STUB 15
ISR_NOERR_STUB 16
ISR_NOERR_STUB 17
ISR_NOERR_STUB 18
ISR_NOERR_STUB 19
ISR_NOERR_STUB 20
ISR_NOERR_STUB 21
ISR_NOERR_STUB 22
ISR_NOERR_STUB 23
ISR_NOERR_STUB 24
ISR_NOERR_STUB 25
ISR_NOERR_STUB 26
ISR_NOERR_STUB 27
ISR_NOERR_STUB 28
ISR_NOERR_STUB 29
ISR_NOERR_STUB 30
ISR_NOERR_STUB 31
ISR_NOERR_STUB 32

/* Default fallback stub for remaining vectors */
.globl isr_stub_default
.type isr_stub_default, @function
isr_stub_default:
    push $0
    push $255
    jmp isr_common
.size isr_stub_default, . - isr_stub_default

.section .note.GNU-stack,"",@progbits
