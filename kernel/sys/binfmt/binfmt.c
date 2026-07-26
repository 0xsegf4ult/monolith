#include <sys/binfmt/binfmt.h>
#include <sys/binfmt/elf.h>
#include <sched/task.h>
#include <sched/task_sys.h>
#include <sched/scheduler.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <sys/smp.h>
#include <libk/list.h>
#include <libk/string.h>
#include <errno.h>
#include <types.h>
#include <cpu.h>

static list_head_t binfmt_list = {&binfmt_list, &binfmt_list};

static binfmt_descriptor_t binfmt_elf =
{
	.name = "ELF",
	.exec = elf_exec,
	.signature = {ELF_MAG0, ELF_MAG1, ELF_MAG2, ELF_MAG3},
	.siglen = 4
};

void binfmt_register(binfmt_descriptor_t* desc)
{
	list_node_init(&desc->list_node);
	list_add(&binfmt_list, &desc->list_node);
}

void binfmt_init()
{
	binfmt_register(&binfmt_elf);
}

void binfmt_exec_task()
{
	struct task* task = smp_current_task();
	int binfd = vfs_open(task->argv[0], 0);
	if(binfd < 0)
		sys_exit(binfd);

	struct stat ex_stat;
	vfs_fstat(binfd, &ex_stat);
	if(!S_ISREG(ex_stat.st_mode))
	{
		vfs_close(binfd);
		sys_exit(-EACCES);
	}

	uint8_t sig[BINFMT_SIGMAX];
	ssize_t r = vfs_read(binfd, (byte*)sig, BINFMT_SIGMAX);
	if(r < 0)
	{
		vfs_close(binfd);
		sys_exit(r);
	}
	vfs_seek(binfd, 0, SEEK_SET);

	binfmt_descriptor_t* fmt;
	int res = -ENOEXEC;
	virtaddr_t entry = 0;
	list_for_each_entry(fmt, &binfmt_list, list_node)
	{
		if(memcmp(fmt->signature, sig, fmt->siglen) == 0)
		{
			res = fmt->exec(binfd, task, &entry);
			break;
		}
	}

	vfs_close(binfd);
	if(res < 0)
		sys_exit(res);
		
	native_switch_to_usermode(task->rsp, entry);
}
