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

struct elf_image
{
	virtaddr_t entry;
	char* interpreter;
	virtaddr_t base;
	virtaddr_t load_bias;
	Elf64_Half phnum;
	Elf64_Half phentsize;
	Elf64_Off phdr_off;
};

static int elf_load(int binfd, struct task* task, struct elf_image* out_img)
{
	out_img->base = VM_USERSPACE_END;

	Elf64_Ehdr header;
	ssize_t r = vfs_read(binfd, (byte*)&header, sizeof(Elf64_Ehdr));
	if(!elf_validate(&header))
		return -ENOEXEC;
	
	if(header.e_type != ET_EXEC && header.e_type != ET_DYN)
	{
		klog("binfmt_elf: only ET_EXEC or ET_DYN is supported!");
		return -ENOTSUP;
	}
	
	Elf64_Phdr* phdrs = kmalloc(header.e_phentsize * header.e_phnum);
	vfs_seek(binfd, header.e_phoff, SEEK_SET);
	vfs_read(binfd, (byte*)phdrs, header.e_phentsize * header.e_phnum);

	out_img->phnum = header.e_phnum;
	out_img->phentsize = header.e_phentsize;
	out_img->phdr_off = header.e_phoff;

	for(Elf64_Phdr* phdr = phdrs; phdr < phdrs + header.e_phnum; phdr++)
	{
		if(phdr->p_type == PT_INTERP)
		{
			char* interpreter = kmalloc(phdr->p_filesz);
			vfs_seek(binfd, phdr->p_offset, SEEK_SET);
			vfs_read(binfd, (byte*)interpreter, phdr->p_filesz);
			out_img->interpreter = interpreter;
			continue;
		}

		if(phdr->p_vaddr < out_img->base)
			out_img->base = phdr->p_vaddr;
	}

	if(header.e_type == ET_DYN)
		out_img->base = 0x7f0000000000;

	bool found_bias = false;
	for(Elf64_Phdr* phdr = phdrs; phdr < phdrs + header.e_phnum; phdr++)
	{
		if(phdr->p_type != PT_LOAD)
			continue;
		
		if(!found_bias)
		{
			out_img->load_bias = out_img->base - phdr->p_vaddr;
			found_bias = true;
		}
		
		uint32_t protflags = PROT_USER | PROT_READ;
		if(phdr->p_flags & PF_W)
			protflags |= PROT_WRITE;
		if(phdr->p_flags & PF_X)
			protflags |= PROT_EXEC;

		virtaddr_t map_vaddr = out_img->load_bias + align_down(phdr->p_vaddr, CONFIG_PAGE_SIZE);
		off_t map_offset = align_down(phdr->p_offset, CONFIG_PAGE_SIZE);
		virtaddr_t align_rem = (out_img->load_bias + phdr->p_vaddr) - map_vaddr;

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
			memset((void*)(file_end_addr), 0, anon_map_start - file_end_addr);

			virtaddr_t anon_map_end = align_up(mem_end_addr, CONFIG_PAGE_SIZE);

			if(anon_map_end - anon_map_start)
			{
				vm_space_map(task->current_vm_space,
				(vm_mapping_info)
				{
					.length = anon_map_end - anon_map_start,
					.prot = protflags,
					.virt_base = out_img->load_bias + anon_map_start
				});
			}
		}
	}
	
	out_img->entry = out_img->load_bias + header.e_entry;
	kfree(phdrs);
	return 0;
}

int elf_exec(int binfd, struct task* task, virtaddr_t* out_entry)
{
	virtaddr_t entry = 0;
	struct elf_image main_img = {};
	int load_status = elf_load(binfd, task, &main_img);
	if(load_status < 0)
		return load_status;

	entry = main_img.entry;

	struct elf_image interp_img = {};
	if(main_img.interpreter)
	{
		int interpfd = vfs_open(main_img.interpreter, 0);
		int interp_status = elf_load(interpfd, task, &interp_img);
		entry = interp_img.entry;
		kfree(main_img.interpreter);
		vfs_close(interpfd);
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
        *(uintptr_t*)task->rsp = main_img.load_bias;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_BASE;

	task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = main_img.entry;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_ENTRY;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = main_img.phnum;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PHNUM;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = main_img.phentsize;
        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = AT_PHENT;

        task->rsp -= sizeof(uintptr_t);
        *(uintptr_t*)task->rsp = main_img.base + main_img.phdr_off;
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
	*(int64_t*)task->rsp = task->argc;

	*out_entry = entry;
	return 0;
}
