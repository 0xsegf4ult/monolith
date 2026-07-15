.section .text
.global reload_segments
.type reload_segments, @function

reload_segments:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	push $0x8
	lea trampoline(%rip), %rax
	push %rax
	lretq
trampoline:
	retq

.global enable_sse
.type enable_sse, @function
enable_sse:
	movq %cr0, %rax
	and $0xFFFB, %ax
	movq %rax, %cr0

	movq %cr4, %rax
	bts $9, %rax # OSFXSR
	bts $10, %rax # OSXMMEXCEPT
	movq %rax, %cr4
	ret

.global enable_xsave
.type enable_xsave, @function
enable_xsave:
	movq %cr4, %rax
	bts $18, %rax # OSXSAVE
	movq %rax, %cr4

	movq $0, %rcx # write to XCR0
	movl $0, %edx
	movl $0, %eax
	bts $0, %eax # XCR0_X87 must be enabled
	bts $1, %eax # XCR0_SSE
	xsetbv
	ret

.global arch_disable_interrupts
.type arch_disable_interrupts, @function
arch_disable_interrupts:
	pushq %rbp
	movq %rsp, %rbp
	pushfq
	popq %rax
	cli
	popq %rbp
	ret

.global arch_restore_flags
.type arch_restore_flags, @function
arch_restore_flags:
	pushq %rbp
	movq %rsp, %rbp
	pushq %rdi
	popfq
	popq %rbp
	ret
