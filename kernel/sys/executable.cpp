#include <sys/executable.hpp>
#include <sys/task.hpp>
#include <sys/err.hpp>

#include <fs/vfs.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <elf.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>

int load_executable(const char* path, task_t* task, vfs::ventry_t* exec_dir)
{
	if(!path)
		return -EINVAL;

	if(!exec_dir)
		exec_dir = task->cwd;

	auto exec_fd = vfs::openat(exec_dir, path);
	if(exec_fd < 0)
		return exec_fd;

	vfs::stat_t ex_stat;
	vfs::fstat(exec_fd, &ex_stat);
	if(!S_ISREG(ex_stat.st_mode))
	{
		vfs::close(exec_fd);
		return -EACCES;
	}

	if(ex_stat.st_size < sizeof(Elf64_Ehdr))
	{
		vfs::close(exec_fd);
		return -ENOEXEC;
	}

	auto* ebuf = (Elf64_Ehdr*)kmalloc(sizeof(Elf64_Ehdr));
	if(!ebuf)
	{
		vfs::close(exec_fd);
		return -ENOMEM;
	}

	vfs::read(exec_fd, (byte*)ebuf, sizeof(Elf64_Ehdr));

	if(!elf_validate(ebuf))
	{
		vfs::close(exec_fd);
		kfree(ebuf);
		return -ENOEXEC;
	}

	task->entry = ebuf->e_entry;
	auto* phdrs = (Elf64_Phdr*)kmalloc(ebuf->e_phentsize * ebuf->e_phnum);
	vfs::seek(exec_fd, ebuf->e_phoff);
	vfs::read(exec_fd, (byte*)phdrs, ebuf->e_phentsize * ebuf->e_phnum);
	auto* phdr = phdrs;
	for(int i = 0; i < ebuf->e_phnum; i++)
	{
		if(phdr->p_type == PT_LOAD)
		{
			uint32_t protflags = PROT_USER | PROT_READ;
			if(phdr->p_flags & PF_W)
				protflags |= PROT_WRITE;
			if(phdr->p_flags & PF_X)
			{
				if(phdr->p_flags & PF_W)
					log::warn("section has W+X flags!");
				protflags |= PROT_EXEC;
			}

			virtaddr_t map_vaddr = align_down(phdr->p_vaddr, ARCH_PAGE_SIZE);
			off_t map_offset = align_down(phdr->p_offset, ARCH_PAGE_SIZE);
			virtaddr_t align_rem = phdr->p_vaddr - map_vaddr;

			if(phdr->p_filesz)
			{
				vm_space_map(task->current_vm_space, 
				{ 
					.length = phdr->p_filesz + align_rem, 
					.prot = protflags,
					.flags = VM_FLAG_FILE | VM_FLAG_COW,
					.virt_base = map_vaddr,
					.offset = map_offset,
					.fd = exec_fd
				});
			}

			if(phdr->p_memsz > phdr->p_filesz)
			{
				auto file_end_addr = phdr->p_vaddr + phdr->p_filesz;
				auto mem_end_addr = phdr->p_vaddr + phdr->p_memsz;

				auto anon_map_start = align_up(file_end_addr, ARCH_PAGE_SIZE);
				auto anon_map_end = align_up(mem_end_addr, ARCH_PAGE_SIZE);

				vm_space_map(task->current_vm_space,
				{
					.length = anon_map_end - anon_map_start,
					.prot = protflags,
					.virt_base = anon_map_start
				});

			}
		}
		phdr++;
	}
	vfs::close(exec_fd);
	kfree(phdrs);
	kfree(ebuf);

	return 0;
}
