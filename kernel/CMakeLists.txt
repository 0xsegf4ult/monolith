set(KERNEL_CFLAGS -ffreestanding -nostdlib -nodefaultlibs)
set(KERNEL_LDFLAGS -nostdlib -nodefaultlibs)
set(KERNEL_SOURCES
	dev/console.c
	dev/efifb_font.c
	dev/efifb.c
	fs/procfs/procfs.c
	fs/ramfs/ramfs.c
	fs/filesystem.c
	fs/generic.c
	fs/initramfs.c
	fs/lookup.c
	fs/super.c
	fs/ventry.c
	fs/vfs.c
	fs/vnode.c
	mm/memory_map.c
	mm/pmm.c
	mm/slab.c
	mm/vmm.c
	libk/string.c
	libk/vsprintf.c
	sched/scheduler.c
	sched/task.c
	sys/clock.c
	sys/device.c
	sys/irq.c
	sys/mutex.c
	sys/timer.c
	init.c
	klog.c
	panic.c
	stackchk.c
)

add_subdirectory("arch/${BUILD_ARCH}")

add_executable(kernel)
target_compile_options(kernel PRIVATE ${KERNEL_CFLAGS})
target_link_options(kernel PRIVATE ${KERNEL_LDFLAGS})
target_include_directories(kernel PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include "${CMAKE_CURRENT_LIST_DIR}/include/arch/${BUILD_ARCH}")
target_sources(kernel PRIVATE ${KERNEL_SOURCES})
set_target_properties(kernel PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/sysroot/boot")
