#include <sys/elf_exec.hpp>
#include <sys/err.hpp>
#include <sys/task.hpp>
#include <fs/vfs.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <klog.hpp>
#include <kstd.hpp>
#include <elf.hpp>
#include <types.hpp>

static bool elf_validate(Elf64_Ehdr& header)
{
	if(header.e_ident[EI_CLASS] != ELF_CLASS64)
		return false;
	if(header.e_ident[EI_DATA] != ELF_DATA2LSB)
		return false;
	if(header.e_machine != EM_X86_64)
		return false;
	if(header.e_ident[EI_VERSION] != EV_CURRENT)
		return false;

	return true;
}

virtaddr_t elf_exec(int binfd, task_t* task)
{
	Elf64_Ehdr header;
	ssize_t r = vfs::read(binfd, (byte*)&header, sizeof(Elf64_Ehdr));

	if(!elf_validate(header))
		return ENOEXEC;	

	if(header.e_type != ET_EXEC)
	{
		log::error("binfmt_elf: only ET_EXEC is supported!");
		return ENOTSUP;
	}

	auto* phdrs = (Elf64_Phdr*)kmalloc(header.e_phentsize * header.e_phnum);
	vfs::seek(binfd, header.e_phoff);
	vfs::read(binfd, (byte*)phdrs, header.e_phentsize * header.e_phnum);

	virtaddr_t base = 0;

	auto* phdr = phdrs;
	for(int i = 0; i < header.e_phnum; i++)
	{
		if(phdr->p_type != PT_LOAD)
		{
			phdr++;
			continue;
		}

		uint32_t protflags = PROT_USER | PROT_READ;
		if(phdr->p_flags & PF_W)
			protflags |= PROT_WRITE;
		if(phdr->p_flags & PF_X)
			protflags |= PROT_EXEC;

		virtaddr_t map_vaddr = align_down(phdr->p_vaddr, ARCH_PAGE_SIZE);
		off_t map_offset = align_down(phdr->p_offset, ARCH_PAGE_SIZE);
		virtaddr_t align_rem = phdr->p_vaddr - map_vaddr;
		if(!base)
			base = map_vaddr;

		if(phdr->p_filesz)
		{
			vm_space_map(task->current_vm_space,
			{
				.length = phdr->p_filesz + align_rem,
				.prot = protflags,
				.flags = VM_FLAG_FILE | VM_FLAG_COW,
				.virt_base = map_vaddr,
				.offset = map_offset,
				.fd = binfd 
			});
		}

		if(phdr->p_memsz > phdr->p_filesz)
		{
			auto file_end_addr = map_vaddr + align_rem + phdr->p_filesz;
			auto mem_end_addr = map_vaddr + align_rem + phdr->p_memsz;

			auto anon_map_start = align_up(file_end_addr, ARCH_PAGE_SIZE);

			memset((void*)file_end_addr, 0, anon_map_start - file_end_addr);

			auto anon_map_end = align_up(mem_end_addr, ARCH_PAGE_SIZE);

			vm_space_map(task->current_vm_space,
			{
				.length = anon_map_end - anon_map_start,
				.prot = protflags,
				.virt_base = anon_map_start
			});
		}

		phdr++;
	}
	

	auto* argv_pointers = (uintptr_t*)kmalloc(sizeof(uintptr_t) * task->argc);
	auto* envp_pointers = (uintptr_t*)kmalloc(sizeof(uintptr_t) * task->envc);

	for(int i = 0; i < task->envc; i++)
	{
		auto str_len = string_length(task->envp[i]);
		for(ssize_t j = str_len; j >= 0; j--)
		{
			task->rsp--;
			*(char*)task->rsp = task->envp[i][j];
		}

		envp_pointers[i] = task->rsp;
	}

	for(int i = 0; i < task->argc; i++)
	{
		auto str_len = string_length(task->argv[i]);
		for(ssize_t j = str_len; j >= 0; j--)
		{
			task->rsp--;
			*(char*)task->rsp = task->argv[i][j];
		}
		argv_pointers[i] = task->rsp;
	}

	task->rsp = align_down(task->rsp, 16);

	auto align_check = task->rsp - (11 * 2 * sizeof(uintptr_t)) - ((task->envc + 1) * sizeof(uintptr_t)) - ((task->argc + 1) * sizeof(uintptr_t)) - sizeof(uintptr_t);
	if(align_check % 16)
	{
		task->rsp -= sizeof(uintptr_t);
		*(uintptr_t*)task->rsp = 0;
	}
	
	task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = 0;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_NULL;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = task->cred.egid;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_EGID;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = task->cred.euid;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_EUID;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = task->cred.gid;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_GID;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = task->cred.uid;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_UID;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = 0x1000;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PAGESZ;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = 0;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_BASE;

	 task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = header.e_entry;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_ENTRY;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = header.e_phnum;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PHNUM;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = header.e_phentsize;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PHENT;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = base + header.e_phoff;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PHDR;

	//envp null
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = 0;
        for(int i = task->envc; i > 0; i--)
        {
                task->rsp -= sizeof(uintptr_t);
                *(uintptr_t*)task->rsp = envp_pointers[i - 1];
        }

        //argv null
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = 0;
        for(int i = task->argc; i > 0; i--)
        {
                task->rsp -= sizeof(uintptr_t);
                *(uintptr_t*)task->rsp = argv_pointers[i - 1];
        }

        // argc
        task->rsp -= sizeof(uintptr_t);
        *(int*)task->rsp = task->argc;

	kfree(envp_pointers);
	kfree(argv_pointers);
	kfree(phdrs);
	return header.e_entry;
}
