.section .text
.global _start
.type _start, @function

.extern main
.extern _exit
_start:
	xorq %rbp, %rbp

	movl (%rsp), %edi # argc
	movq 8(%rsp), %rsi # argv

	xorq %rax, %rax
	call main

	movq %rax, %rdi
	xorq %rax, %rax
	call _exit
