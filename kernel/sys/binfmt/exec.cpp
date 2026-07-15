#include <sys/exec.hpp>
#include <sys/elf_exec.hpp>
#include <sys/err.hpp>
#include <sys/task.hpp>
#include <sys/smp.hpp>
#include <sys/syscall.hpp>
#include <arch/generic.hpp>
#include <fs/vfs.hpp>
#include <mm/slab.hpp>
#include <list.hpp>
#include <elf.hpp>
#include <kstd.hpp>
#include <klog.hpp>

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
	list_node_init(desc->list_node);
	list_add(binfmt_list, desc->list_node);
}

void binfmt_init()
{
	binfmt_register(&binfmt_elf);
}

void exec_task()
{
	auto* task = smp_current_task();
	
	auto binfd = vfs::open(task->argv[0]);
	if(binfd < 0)
		sys_exit(binfd);

	vfs::stat_t ex_stat;
	vfs::fstat(binfd, &ex_stat);
	if(!S_ISREG(ex_stat.st_mode))
	{
		vfs::close(binfd);
		sys_exit(-EACCES);
	}

	uint8_t sig[BINFMT_SIGMAX];
	auto r = vfs::read(binfd, (byte*)sig, BINFMT_SIGMAX);
	if(r < 0)
	{
		vfs::close(binfd);
		sys_exit(r);
	}
	vfs::seek(binfd, 0);

	binfmt_descriptor_t* fmt;
	virtaddr_t res = ENOEXEC;
	list_for_each_entry(fmt, binfmt_list, list_node)
	{
		if(memcmp(fmt->signature, sig, fmt->siglen) == 0)
		{
			res = fmt->exec(binfd, task);
			break;
		}
	}
	
	if(task->envp && task->pid > 1)
	{
		for(int i = 0; i < task->envc; i++)
			kfree(task->envp[i]);

		kfree(task->envp);
		task->envp = nullptr;
	}

	if(task->pid > 1)
	{
		for(int i = 0; i < task->argc; i++)
			kfree(task->argv[i]);

		kfree(task->argv);
		task->argv = nullptr;
	}

	vfs::close(binfd);
	
	if(res < 4096)
		sys_exit(-(int)res);
	
	arch_switch_to_usermode(task->rsp, res);
}
