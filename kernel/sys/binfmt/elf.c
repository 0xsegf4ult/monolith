#include <sys/binfmt/elf.h>
#include <sched/task.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <libk/string.h>
#include <config.h>
#include <errno.h>
#include <klog.h>
#include <types.h>

static bool elf_validate(Elf64_Ehdr* header)
{
	if(header->e_ident[EI_CLASS] != ELF_CLASS64)
		return false;
	if(header->e_ident[EI_DATA] != ELF_DATA2LSB)
		return false;
	if(header->e_machine != EM_X86_64)
		return false;
	if(header->e_ident[EI_VERSION] != EV_CURRENT)
		return false;

	return true;
}

int elf_exec(int binfd, struct task* task, virtaddr_t* out_entry)
{
	Elf64_Ehdr header;
	ssize_t r = vfs_read(binfd, (byte*)&header, sizeof(Elf64_Ehdr));

	if(!elf_validate(&header))
		return -ENOEXEC;

	if(header.e_type != ET_EXEC)
	{
		klog("binfmt_elf: only ET_EXEC is supported!");
		return -ENOTSUP;
	}

	Elf64_Phdr* phdrs = kmalloc(header.e_phentsize * header.e_phnum);
	vfs_seek(binfd, header.e_phoff, SEEK_SET);
	vfs_read(binfd, (byte*)phdrs, header.e_phentsize * header.e_phnum);

	virtaddr_t base = 0;

	Elf64_Phdr* phdr = phdrs;
	for(Elf64_Phdr* phdr = phdrs; phdr < phdrs + header.e_phnum; phdr++)
	{
		if(phdr->p_type != PT_LOAD)
			continue;

		uint32_t protflags = PROT_USER | PROT_READ;
		if(phdr->p_flags & PF_W)
			protflags |= PROT_WRITE;
		if(phdr->p_flags & PF_X)
			protflags |= PROT_EXEC;

		virtaddr_t map_vaddr = align_down(phdr->p_vaddr, CONFIG_PAGE_SIZE);
		off_t map_offset = align_down(phdr->p_offset, CONFIG_PAGE_SIZE);
		virtaddr_t align_rem = phdr->p_vaddr - map_vaddr;
		if(!base)
			base = map_vaddr;

		if(phdr->p_filesz)
		{
			vm_space_map(task->current_vm_space,
			(vm_mapping_info)
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
			virtaddr_t file_end_addr = map_vaddr + align_rem + phdr->p_filesz;
			virtaddr_t mem_end_addr = map_vaddr + align_rem + phdr->p_memsz;

			virtaddr_t anon_map_start = align_up(file_end_addr, CONFIG_PAGE_SIZE);
			memset((void*)file_end_addr, 0, anon_map_start - file_end_addr);

			virtaddr_t anon_map_end = align_up(mem_end_addr, CONFIG_PAGE_SIZE);

			vm_space_map(task->current_vm_space,
			(vm_mapping_info)
			{
				.length = anon_map_end - anon_map_start,
				.prot = protflags,
				.virt_base = anon_map_start
			});
		}
	}

	virtaddr_t align_check = task->rsp - (11 * 2 * sizeof(uintptr_t)) - ((task->envc + 1) * sizeof(uintptr_t)) - ((task->argc + 1) * sizeof(uintptr_t)) - sizeof(uintptr_t);
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
        *(uintptr_t*)task->rsp = CONFIG_PAGE_SIZE;
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

	task->rsp -= sizeof(uintptr_t);
	*(uintptr_t*)task->rsp = 0;
	for(int i = task->envc; i > 0; i--)
	{
		task->rsp -= sizeof(uintptr_t);
		*(char**)task->rsp = task->envp[i - 1];
	}

	task->rsp -= sizeof(uintptr_t);
	*(uintptr_t*)task->rsp = 0;
	for(int i = task->argc; i > 0; i--)
	{
		task->rsp -= sizeof(uintptr_t);
		*(char**)task->rsp = task->argv[i - 1];
	}

	task->rsp -= sizeof(uintptr_t);
	*(int*)task->rsp = task->argc;

	kfree(phdrs);
	*out_entry = header.e_entry;
	return 0;
}
