#include <sys/executable.hpp>
#include <sys/process.hpp>

#include <fs/vfs.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <lib/elf.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

int load_executable(int exec_fd, process_t* proc)
{
	auto* ebuf = (Elf64_Ehdr*)kmalloc(sizeof(Elf64_Ehdr));
	vfs::read(exec_fd, (byte*)ebuf, sizeof(Elf64_Ehdr));
	if(!elf_validate(ebuf))
		return -1;

	proc->entry = ebuf->e_entry;
	auto* phdrs = (Elf64_Phdr*)kmalloc(ebuf->e_phentsize * ebuf->e_phnum);
	vfs::seek(exec_fd, ebuf->e_phoff);
	vfs::read(exec_fd, (byte*)phdrs, ebuf->e_phentsize * ebuf->e_phnum);
	auto* phdr = phdrs;
	for(int i = 0; i < ebuf->e_phnum; i++)
	{
		log::debug("ELF phdr type {} flags {:b} offset {:x}", phdr->p_type, phdr->p_flags, phdr->p_offset);
		if(phdr->p_type == PT_LOAD)
		{
			log::debug("LOAD {}{}{}", (phdr->p_flags & PF_R) ? 'R' : '-', (phdr->p_flags & PF_W) ? 'W' : '-', (phdr->p_flags & PF_X) ? 'X' : '-');
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
			log::debug("vmalloc {:x} size {:x}", aligned_vaddr, phdr->p_memsz + page_offset);
			proc->vm_space->alloc_placed(aligned_vaddr, phdr->p_memsz + page_offset, vmflags);
			virtaddr_t user_page = proc->vm_space->get_mapping(aligned_vaddr) + mm::direct_mapping_offset;
			log::debug("pages mapped at {:x}", user_page);
			vfs::seek(exec_fd, phdr->p_offset);
			log::debug("copy {:x} bytes from EHDR + {:x}", phdr->p_filesz, phdr->p_offset);
			vfs::read(exec_fd, (byte*)user_page + page_offset, phdr->p_filesz);
			auto zerocount = phdr->p_memsz - phdr->p_filesz;
			if(zerocount)
			{
				log::debug("zeroing {:x} bytes at {:x}", zerocount, phdr->p_memsz);
				memset((void*)(user_page + page_offset + phdr->p_filesz), 0, zerocount);
			}
		}
		phdr++;
	}
	
	kfree(phdrs);
	kfree(ebuf);

	return 0;
}
