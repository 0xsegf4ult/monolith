add_executable(vmmonolith)

target_compile_options(vmmonolith PRIVATE -target x86_64-none-elf -ffreestanding -fno-rtti -fno-exceptions -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -nostdlib -nostdlib++ -nodefaultlibs -fPIC)
target_link_options(vmmonolith PRIVATE -T ${CMAKE_CURRENT_LIST_DIR}/arch/x86_64/linker.ld -fuse-ld=lld -nostdlib -nostdlib++ -nodefaultlibs)
target_include_directories(vmmonolith PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_sources(vmmonolith PRIVATE
	arch/x86_64/acpi.cpp
	arch/x86_64/apic.cpp
	arch/x86_64/arch.S
	arch/x86_64/cpu.cpp
	arch/x86_64/context.S
	arch/x86_64/context.cpp
	arch/x86_64/gdt.cpp
	arch/x86_64/interrupts.cpp
	arch/x86_64/isr_stubs.S
	arch/x86_64/mmu.cpp
	arch/x86_64/serial.cpp
	arch/x86_64/smp.cpp
	arch/x86_64/smpboot.S
	arch/x86_64/timer.cpp
	dev/block/block.cpp
	dev/block/disk.cpp
	dev/block/partition.cpp
	dev/character.cpp
	dev/console.cpp
	dev/efifb.cpp
	dev/efifb_font.cpp
	dev/nvme/nvme.cpp
	dev/pcie.cpp
	dev/ps2.cpp
	dev/pseudo.cpp
	dev/rtc.cpp
	dev/tty.cpp
	fs/fat/fatfs.cpp
	fs/procfs/procfs.cpp
	fs/ramfs/ramfs.cpp
	fs/generic.cpp
	fs/filesystem.cpp
	fs/lookup.cpp
	fs/super.cpp
	fs/vfs.cpp
	fs/ventry.cpp
	fs/vnode.cpp
	mm/address_space.cpp
	mm/memory_map.cpp
	mm/pmm.cpp
	mm/slab.cpp
	mm/vmm.cpp
	sys/executable.cpp
	sys/mutex.cpp
	sys/task.cpp
	sys/time.cpp
	sys/scheduler.cpp
	sys/signal.cpp
	sys/syscall.cpp
	sys/waitqueue.cpp
	elf.cpp
	init.cpp
	initramfs.cpp
	klog.cpp
	kstd.cpp
	panic.cpp
	stackchk.cpp)
set_target_properties(vmmonolith PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/boot")
