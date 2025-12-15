add_executable(vmmonolith)

target_compile_options(vmmonolith PRIVATE -target x86_64-none-elf -ffreestanding -fno-rtti -fno-exceptions -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -nostdlib -nostdlib++ -nodefaultlibs -fPIC)
target_link_options(vmmonolith PRIVATE -T ${CMAKE_CURRENT_LIST_DIR}/init/linker.ld -fuse-ld=lld -nostdlib -nostdlib++ -nodefaultlibs)
target_include_directories(vmmonolith PRIVATE ${CMAKE_SOURCE_DIR}/buildroot/usr/include ${CMAKE_CURRENT_LIST_DIR})
target_sources(vmmonolith PRIVATE
	arch/x86_64/cpu.cpp
	arch/x86_64/gdt.S
	arch/x86_64/isr_stubs.S
	arch/x86_64/interrupts.cpp
	arch/x86_64/mmu.cpp
	arch/x86_64/serial.cpp
	mm/address_space.cpp
	mm/memory_map.cpp
	mm/pmm.cpp
	mm/slab.cpp
	mm/vmm.cpp
	lib/kstd.cpp
	lib/klog.cpp
	init/init.cpp)
set_target_properties(vmmonolith PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/boot")
