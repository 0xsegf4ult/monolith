set(KERNEL_CFLAGS ${KERNEL_CFLAGS} -mcmodel=kernel -mno-red-zone -mgeneral-regs-only PARENT_SCOPE)
set(KERNEL_LDFLAGS ${KERNEL_LDFLAGS} -T ${CMAKE_CURRENT_LIST_DIR}/linker.ld PARENT_SCOPE)
set(KERNEL_SOURCES ${KERNEL_SOURCES} 
	arch/x86_64/acpi.c
	arch/x86_64/boot.c
	arch/x86_64/context.S
	arch/x86_64/cpu.c
	arch/x86_64/gdt.c
	arch/x86_64/gdt.S
	arch/x86_64/ioapic.c
	arch/x86_64/idt.c
	arch/x86_64/isr_stubs.S
	arch/x86_64/interrupts.c
	arch/x86_64/lapic.c
	arch/x86_64/memmap.c
	arch/x86_64/mmu.c
	arch/x86_64/pit.c
	arch/x86_64/simd.S
	arch/x86_64/smpboot.S
	PARENT_SCOPE)
