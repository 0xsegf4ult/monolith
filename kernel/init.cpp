#include <dev/console.hpp>
#include <dev/efifb.hpp>
#include <dev/pcie.hpp>
#include <dev/ps2.hpp>
#include <dev/pseudo.hpp>
#include <dev/rtc.hpp>

#include <fs/fat/fatfs.hpp>
#include <fs/procfs/procfs.hpp>
#include <fs/super.hpp>
#include <fs/vfs.hpp>

#include <initramfs.hpp>

#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <net/arp.hpp>
#include <net/netdev.hpp>
#include <net/ipv4/ipv4.hpp>

#include <sys/exec.hpp>
#include <sys/task.hpp>
#include <sys/scheduler.hpp>
#include <sys/time.hpp>

#include <sys/spinlock.hpp>

#include <init.hpp>
#include <klog.hpp>
#include <kstd.hpp>
#include <panic.hpp>

boot_info_t boot_info;

static char* initargs[] = {"/bin/init", nullptr};

static void spawn_init()
{
	auto* init_proc = process_userspace_new(initargs[0], reinterpret_cast<virtaddr_t>(exec_task));
       	init_proc->argc = 1;
	init_proc->envc = 0;
	init_proc->argv = initargs;
	
	int confd = vfs::open("/dev/console", 0);
	init_proc->open_files[0] = confd;
	init_proc->open_files[1] = vfs::dup(confd);
	init_proc->open_files[2] = vfs::dup(confd);

	sched_add_ready(init_proc);
}

void kernel_main()
{
	vfs::init();
	vfs::mkdir("/dev", 0755);
	vfs::mkdir("/proc", 0755);
	
	procfs_init();
	vmm_late_init();
	
	pseudo_init();
	ps2_init();
	efifb_init(boot_info.fb);
	console_init();

	rtc_init();
	auto time = rtc_read();
	time_set_boottime(time);
	log::info("rtc: updated system time to {}", time);

	fatfs_init();

	netdev_init();
	arp_init();
	ipv4_init();

	pcie_init();
	binfmt_init();

	log::info("initramfs [{:#x} - {:#x}]", boot_info.initramfs_address, boot_info.initramfs_address + boot_info.initramfs_size);
	mmu_map_range(vm_get_kernel_space()->mmu_root, (physaddr_t)boot_info.initramfs_address - VM_DMAP_BASE, boot_info.initramfs_address, boot_info.initramfs_size, PROT_READ, 0);
	initramfs_unpack((byte*)boot_info.initramfs_address, boot_info.initramfs_size);

	vfs::mount(nullptr, "/proc", "procfs");

	spawn_init();	
}
