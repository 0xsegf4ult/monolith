.code64
.section .text
.global gdt_load
.type gdt_load, @function

gdt_load:
	lgdt (%rdi)
	pushq $0x8
	leaq trampoline(%rip), %rax
	pushq %rax
	lretq
trampoline:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	retq
	int3	
