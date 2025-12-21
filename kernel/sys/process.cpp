#include <sys/process.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

static uint32_t next_pid = 1;

process_t* create_process(const char* name, virtaddr_t entry, void* arg)
{
	auto* process = (process_t*)kmalloc(sizeof(process_t));
	strncpy(process->name, name, 32);
	process->pid = next_pid++;
	process->vm_space = (address_space*)kmalloc(sizeof(address_space));
	process->vm_space->init();
	clone_kernel_vm(process->vm_space);
	process->status = process_status::ready;
	process->next = nullptr;

        auto stack_alloc = pmm_allocate() + mm::direct_mapping_offset;
	auto* stack_ptr = reinterpret_cast<uint64_t*>(stack_alloc + 0x1000);
	*(--stack_ptr) = entry;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;

	process->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
	process->rsp = 0;

	return process;	
}
