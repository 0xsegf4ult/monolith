.macro push_all_registers
        pushq %rax
        pushq %rbx
        pushq %rcx
        pushq %rdx
        pushq %rsi
        pushq %rdi
        pushq %rbp
        pushq %r8
        pushq %r9
        pushq %r10
        pushq %r11
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15
.endm

.macro pop_all_registers
        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %r11
        popq %r10
        popq %r9
        popq %r8
        popq %rbp
        popq %rdi
        popq %rsi
        popq %rdx
        popq %rcx
        popq %rbx
        popq %rax
.endm

.macro interrupt_no_err intr
        .align 16
        pushq $0
        pushq $\intr
        jmp __common_exception
.endm

.macro interrupt_err intr
        .align 16
        pushq $\intr
        jmp __common_exception
.endm

.macro isr_insert intr
        .section .text
	.align 16
	pushq $0
	pushq $\intr
	jmp __common_interrupt
.endm

.section .text

.extern exception_handler
__common_exception:
	cmpq $0x8, 24(%rsp)
	je ex_ring0_enter
	swapgs
ex_ring0_enter:
	push_all_registers

	cld

	movq %rsp, %rdi
	movq %rsp, %rbp
	call exception_handler

	pop_all_registers

	cmpq $0x8, 24(%rsp)
	je ex_ring0_exit
	swapgs
ex_ring0_exit:
	addq $16, %rsp
	iretq

.extern interrupt_handler
__common_interrupt:
	cmpq $8, 24(%rsp)
	je irq_ring0_enter
	swapgs
irq_ring0_enter:
	push_all_registers

	cld
	movq %rsp, %rdi
	movq %rsp, %rbp
	call interrupt_handler

	pop_all_registers

	cmpq $0x8, 24(%rsp)
	je irq_ring0_exit
	swapgs
irq_ring0_exit:
	addq $16, %rsp
	iretq

.global isr_stubs
.align 16
isr_stubs:
interrupt_no_err        0
interrupt_no_err        1
interrupt_no_err        2
interrupt_no_err        3
interrupt_no_err        4
interrupt_no_err        5
interrupt_no_err        6
interrupt_no_err        7
interrupt_err           8
interrupt_no_err        9
interrupt_err           10
interrupt_err           11
interrupt_err           12
interrupt_err           13
interrupt_err           14
interrupt_no_err        15
interrupt_no_err        16
interrupt_err           17
interrupt_no_err        18
interrupt_no_err        19
interrupt_no_err        20
interrupt_no_err        21
interrupt_no_err        22
interrupt_no_err        23
interrupt_no_err        24
interrupt_no_err        25
interrupt_no_err        26
interrupt_no_err        27
interrupt_no_err        28
interrupt_no_err        29
interrupt_err           30
interrupt_no_err        31

.altmacro
.set i, 32
.rept 224
        isr_insert %i
        .set i, i + 1
.endr
