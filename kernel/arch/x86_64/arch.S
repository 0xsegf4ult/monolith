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
