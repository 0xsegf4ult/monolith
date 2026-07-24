#include <init.h>

#include <dev/console.h>
#include <dev/efifb.h>
#include <dev/ps2.h>
#include <dev/pseudo.h>
#include <dev/tty.h>

#include <fs/procfs/procfs.h>
#include <fs/initramfs.h>
#include <fs/vfs.h>
#include <mm/mmu.h>
#include <mm/vmm.h>

#include <sched/scheduler.h>
#include <sched/task.h>
#include <sys/binfmt/binfmt.h>

#include <libk/string.h>

#include <klog.h>
#include <panic.h>

boot_info_t boot_info;

static const char* initargs[] = {"/usr/bin/init", nullptr};

static void spawn_init()
{
	struct task* init_proc = process_userspace_new(initargs[0], (virtaddr_t)binfmt_exec_task);
	init_proc->argc = 1;
	init_proc->envc = 0;
	int res = task_copy_args(init_proc, initargs, nullptr);
	if(res < 0)
		panic("failed to start init: %d\n", res);

	int confd = vfs_open("/dev/console", 0);
	init_proc->open_files[0] = confd;
	init_proc->open_files[1] = vfs_dup(confd);
	init_proc->open_files[2] = vfs_dup(confd);

	sched_add_ready(init_proc);
}

void kernel_main()
{
	vfs_init();
	vfs_mkdir("/dev", 0755);

	efifb_init(&boot_info.fb);
	ps2_init();
	console_init();
	pseudo_init();

	vfs_mkdir("/proc", 0755);
	procfs_init();
	vmm_late_init();

	binfmt_init();

	mmu_map_range(vm_get_kernel_space()->mmu_root, boot_info.initramfs_address - VM_DMAP_BASE, boot_info.initramfs_address, boot_info.initramfs_size, PROT_READ, 0);
	initramfs_unpack((byte*)boot_info.initramfs_address, boot_info.initramfs_size);	

	vfs_mount(nullptr, "/proc", "procfs");
	spawn_init();
}
