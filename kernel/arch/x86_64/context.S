.section .text
.global arch_context_switch
.type arch_context_switch, @function

arch_context_switch:
	pushq %rbx # System V ABI preserved registers
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, (%rdi) # previous_rsp0
	movq %rsi, %rsp # current_rsp0

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx

	retq
	int3

.global arch_switch_to_usermode
.type arch_switch_to_usermode, @function

arch_switch_to_usermode:
	push $0x23 # USER_DS | DPL_USER 
	push %rdi # rsp
	push $0x202 # RFLAGS IF | RF
	push $0x1b # USER_CS | DPL_USER
	push %rsi # rip
	iretq
	int3
	
