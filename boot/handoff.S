.text
.globl enter_kernel
.type enter_kernel, @function
enter_kernel:
    cli
    cld
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsp
    and $-16, %rsp
    xor %ebp, %ebp
    call *%rax
    cli
1:
    hlt
    jmp 1b
.size enter_kernel, . - enter_kernel
.section .note.GNU-stack,"",@progbits
