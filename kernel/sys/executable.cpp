#include <sys/executable.hpp>
#include <sys/task.hpp>
#include <sys/err.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

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

			uint64_t page_offset = phdr->p_vaddr % 0x1000;
			virtaddr_t aligned_vaddr = phdr->p_vaddr - page_offset;
			vm_space_map(task->current_vm_space, 
			{ 
				.length = phdr->p_memsz + page_offset, 
				.prot = protflags,
				.flags = VM_FLAG_ALLOCATED,
				.virt_base = aligned_vaddr
			});
			vfs::seek(exec_fd, phdr->p_offset);

			virtaddr_t cur_page = aligned_vaddr;
			size_t cur_offset = page_offset;
			size_t wr_len = phdr->p_filesz;
			while(wr_len)
			{
				virtaddr_t user_page = vm_space_get_mapping(task->current_vm_space, cur_page).base + cur_offset + VM_DMAP_BASE;
				auto read_count = 0x1000 - cur_offset;
				if(read_count > wr_len)
					read_count = wr_len;

	//			log::debug("read {:x} bytes to {:x} -> {:x}", read_count, user_page, cur_page + cur_offset);
				vfs::read(exec_fd, (byte*)user_page, read_count);
				cur_page += read_count + cur_offset;
				cur_offset = 0;
				wr_len -= read_count;
			}

			wr_len += (phdr->p_memsz - phdr->p_filesz);
			while(wr_len)
			{
				virtaddr_t user_page = vm_space_get_mapping(task->current_vm_space, cur_page).base + VM_DMAP_BASE + (cur_page % 0x1000);
				auto read_count = 0x1000;
				if(read_count > wr_len)
					read_count = wr_len;
				//log::debug("zero {:x} bytes at {:x} -> {:x}", read_count, user_page, cur_page);
				memset((void*)user_page, 0, read_count);
				cur_page += read_count;
				wr_len -= read_count;
			}
		}
		phdr++;
	}
	vfs::close(exec_fd);
	kfree(phdrs);
	kfree(ebuf);

	return 0;
}
