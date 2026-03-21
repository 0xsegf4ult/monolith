.section .text
.global arch_context_switch
.type arch_context_switch, @function

.extern cpu_switch_process;
arch_context_switch:
	pushq %rbp # System V ABI preserved registers
	pushq %rbx 
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, 40(%rdi) # next (process_t->rsp0)
	movq 40(%rsi), %rsp # last 

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp

	jmp cpu_switch_process

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
	
