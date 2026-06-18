#include <sys/executable.hpp>
#include <sys/thread.hpp>
#include <sys/err.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

#include <fs/vfs.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <lib/elf.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

int load_executable(const char* path, thread_t* thr, vfs::ventry_t* exec_dir)
{
	if(!path)
		return -EINVAL;

	if(!exec_dir)
		exec_dir = thr->cwd;

	auto exec_fd = vfs::openat(exec_dir, path);
	if(exec_fd < 0)
		return exec_fd;

	vfs::stat_t ex_stat;
	vfs::fstat(exec_fd, &ex_stat);
	if(!S_ISREG(ex_stat.mode))
	{
		vfs::close(exec_fd);
		return -EACCES;
	}

	if(ex_stat.size < sizeof(Elf64_Ehdr))
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

	thr->entry = ebuf->e_entry;
	auto* phdrs = (Elf64_Phdr*)kmalloc(ebuf->e_phentsize * ebuf->e_phnum);
	vfs::seek(exec_fd, ebuf->e_phoff);
	vfs::read(exec_fd, (byte*)phdrs, ebuf->e_phentsize * ebuf->e_phnum);
	auto* phdr = phdrs;
	for(int i = 0; i < ebuf->e_phnum; i++)
	{
		if(phdr->p_type == PT_LOAD)
		{
			uint64_t vmflags = vm_user;
			if(phdr->p_flags & PF_W)
				vmflags |= vm_write;
			if(phdr->p_flags & PF_X)
			{
				if(phdr->p_flags & PF_W)
					log::warn("section has W+X flags!");
				vmflags |= vm_exec;
			}

			uint64_t page_offset = phdr->p_vaddr % 0x1000;
			virtaddr_t aligned_vaddr = phdr->p_vaddr - page_offset;
			thr->vm_space->alloc_placed(aligned_vaddr, phdr->p_memsz + page_offset, vmflags | vm_present);
			vfs::seek(exec_fd, phdr->p_offset);

			virtaddr_t cur_page = aligned_vaddr;
			size_t cur_offset = page_offset;
			size_t wr_len = phdr->p_filesz;
			while(wr_len)
			{
				virtaddr_t user_page = thr->vm_space->get_mapping(cur_page) + cur_offset + mm::direct_mapping_offset;
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
				virtaddr_t user_page = thr->vm_space->get_mapping(cur_page) + mm::direct_mapping_offset + (cur_page % 0x1000);
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
