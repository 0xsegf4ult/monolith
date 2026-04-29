.section ".rodata", "a", @progbits
.align 4
.code16
.global ap_trampoline_start
ap_trampoline_start:
	cli
	cld
	ljmpw $0x0, $0x1080
.align 16
pm_gdt:
	.long 0, 0
	.long 0x0000FFFF, 0x00CF9A00 # cs
	.long 0x0000FFFF, 0x008F9200 # ds
	.long 0, 0
pm_gdtr:
	.word pm_gdtr - pm_gdt - 1
	.long 0x1010
	.long 0, 0
.align 16
lm_gdt:
	.long 0, 0
	.long 0x0000FFFF, 0x00AF9A00 #0x00AF9800 # cs
	.long 0x0000FFFF, 0x00CF9200 # ds
	.long 0, 0
lm_gdtr:
	.word lm_gdtr - lm_gdt - 1
	.long 0x1040
.align 64
start16:
	xorw %ax, %ax
	movw %ax, %ds
	lgdtl 0x1030
	movl $1, %eax # CR0_PE
	bts $4, %eax # CR0_ET
	movl %eax, %cr0
	ljmpl $8, $0x10a0
.align 32
.code32	
start32:
	movw $16, %ax
	movw %ax, %ds
	movw %ax, %ss

	movl 0x1ff0, %eax # PML4 address 
	movl %eax, %cr3
	
	movl %cr4, %eax
	bts $5, %eax # CR4_PAE	
	movl %eax, %cr4

	movl $0xc0000080, %ecx
	rdmsr
	bts $8, %eax # EFER_LME
	bts $11, %eax # EFER_NXE
	wrmsr

	movl %cr0, %eax
	bts $31, %eax # CR0_PG
	bts $16, %eax # CR0_WP
	movl %eax, %cr0 

	lgdtl 0x1060
	ljmp $8, $0x1100
.align 32
.code64
start64:
	movw $16, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0, %ax
	movw %ax, %fs
	movw %ax, %gs

	# AP LAPIC ID
	movl $1, %eax
	cpuid
	shrl $24, %ebx	

	movl %ebx, %edi
	
	# RSP for current AP is at 0x1fd0 - (LAPIC_ID * 8)
	
	shll $3, %ebx	
	movq $0x1fd0, %rsi
	subq %rbx, %rsi

	movq (%rsi), %rsp
	xorq %rbp, %rbp

	callq *0x1fe0
trampoline_end:
