set(KERNEL_CFLAGS ${KERNEL_CFLAGS} -mcmodel=kernel -mno-red-zone -mgeneral-regs-only PARENT_SCOPE)
set(KERNEL_LDFLAGS ${KERNEL_LDFLAGS} -T ${CMAKE_CURRENT_LIST_DIR}/linker.ld PARENT_SCOPE)
set(KERNEL_SOURCES ${KERNEL_SOURCES} 
	arch/x86_64/arch.S
	arch/x86_64/boot.c
	arch/x86_64/cpu.c
	arch/x86_64/gdt.c
	arch/x86_64/gdt.S
	PARENT_SCOPE)
