#include <sys/process.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>

#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <fs/vfs.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

static uint32_t next_pid = 1;
typedef void (*entry_function_t)();

void thread_entry_stub()
{
	CPU::enable_interrupts();
	auto self = CPU::get_current()->get_current_process();
/*
	log::debug("started process {}", self->name);
	log::debug("entrypoint {:#x}", self->entry);
	log::debug("rsp0 {:#x} rsp3 {:#x}", self->rsp0, self->rsp);
*/
	if(!self->rsp)
	{
		(reinterpret_cast<entry_function_t>(self->entry))();
	}
	else
	{
		arch_switch_to_usermode(self->rsp, self->entry);
	}
}

constexpr size_t kernel_stack_size = 0x1000;
constexpr size_t user_stack_size = 0x4000;

process_t* create_process(const char* name, bool is_user)
{
	auto* process = (process_t*)kmalloc(sizeof(process_t));
	strncpy(process->name, name, 32);
	process->pid = next_pid++;
	process->vm_space = (address_space*)kmalloc(sizeof(address_space));
	process->vm_space->init();
	clone_kernel_vm(process->vm_space);
	process->status = process_status::ready;
	process->next = nullptr;

//	get_kernel_vmspace()->dump_objects();
	auto kstack_alloc = vmalloc(kernel_stack_size, vm_write);
	auto* stack_ptr = reinterpret_cast<uint64_t*>(kstack_alloc + kernel_stack_size);
	*(--stack_ptr) = reinterpret_cast<virtaddr_t>(thread_entry_stub);
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;

	process->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
	process->rsp = 0;
	process->rsp0_top = process->rsp0;
	process->parent = nullptr;
	process->children = nullptr;
	process->sibling = nullptr;
	process->cwd = vfs::get_root_dentry();

	for(int i = 0; i < 32; i++)
		process->open_files[i] = -1;

	if(is_user)
	{
		process->rsp = process->vm_space->alloc(user_stack_size, vm_write | vm_user) + user_stack_size - 8;
		log::debug("allocated user stack {:#x}", process->rsp);
	}

	return process;	
}

void destroy_process(process_t* proc)
{
	proc->vm_space->destroy();
	kfree(proc->vm_space);

	kfree(proc);
}
