set(KERNEL_CFLAGS -ffreestanding -nostdlib -nodefaultlibs -Wno-packed-bitfield-compat -fstack-protector-strong)
set(KERNEL_LDFLAGS -nostdlib -nodefaultlibs)
set(KERNEL_SOURCES
	dev/console.c
	dev/disk.c
	dev/efifb_font.c
	dev/efifb.c
	dev/net/e1000/e1000.c
	dev/nvme/nvme.c
	dev/pcie/pcie.c
	dev/pcie/pci_class.c
	dev/ps2.c
	dev/pseudo.c
	dev/rtc.c
	dev/tty.c
	fs/fat/fatfs.c
	fs/procfs/procfs.c
	fs/ramfs/ramfs.c
	fs/filesystem.c
	fs/generic.c
	fs/initramfs.c
	fs/lookup.c
	fs/super.c
	fs/ventry.c
	fs/vfs.c
	fs/vfs_sys.c
	fs/vnode.c
	mm/memory_map.c
	mm/pmm.c
	mm/slab.c
	mm/vm_syscall.c
	mm/vmm.c
	net/ipv4/ipv4.c
	net/ipv4/raw.c
	net/ipv4/route.c
	net/ipv4/udp.c
	net/arp.c
	net/ether.c
	net/netdev.c
	net/socket.c
	net/socket_sys.c
	libk/string.c
	libk/vsprintf.c
	sched/scheduler.c
	sched/signal.c
	sched/task.c
	sched/task_sleep.c
	sched/task_sys.c
	sched/waitqueue.c
	sys/binfmt/binfmt.c
	sys/binfmt/elf.c
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
