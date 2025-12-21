#include <sys/scheduler.hpp>
#include <sys/process.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

static process_t* current_process = nullptr;
static virtaddr_t boot_stack = 0;

static process_t* idle_process = nullptr;
static process_t* ready_list_head = nullptr;
static process_t* ready_list_tail = nullptr;

void idle_process_entry(void* arg)
{
	log::info("sched: running on bsp");
	CPU::enable_interrupts();

	for(;;)
	{
		asm volatile("hlt");
	}
}

void schedule()
{
	if(ready_list_head)
	{
		if(current_process->status == process_status::running)
		{
			current_process->status = process_status::ready;
			ready_list_tail->next = current_process;
			ready_list_tail = current_process;
		}
	
		process_t* last = current_process;	
		process_t* proc = ready_list_head;
		current_process = proc;
		ready_list_head = proc->next;
		if(ready_list_tail == proc)
			ready_list_tail = ready_list_head;

		current_process->status = process_status::running; 
		current_process->vm_space->switch_to();

		// should happen as part of ctx switch to avoid races?
		CPU::enable_interrupts();
		arch_context_switch(&last->rsp0, current_process->rsp0);
	}	
}

void sched_init()
{
}

void sched_start()
{
	idle_process = (process_t*)kmalloc(sizeof(process_t));
        strncpy(idle_process->name, "idle", 32);
        idle_process->pid = 0;
        idle_process->vm_space = get_kernel_vmspace();
        idle_process->status = process_status::ready;
	idle_process->next = nullptr;

        auto stack_alloc = pmm_allocate() + mm::direct_mapping_offset;
        auto* stack_ptr = reinterpret_cast<uint64_t*>(stack_alloc + 0x1000);
        *(--stack_ptr) = reinterpret_cast<virtaddr_t>(idle_process_entry);
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;

        idle_process->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
        idle_process->rsp = 0;
	
	current_process = idle_process;
	current_process->status = process_status::running;

	arch_context_switch(&boot_stack, current_process->rsp0);

	panic("failed to start idle process");
}

void sched_add_ready(process_t* proc)
{
	if(!ready_list_head)
	{
		ready_list_head = proc;
		ready_list_tail = proc;
	}
	else
	{
		ready_list_tail->next = proc;
		ready_list_tail = proc;
	}
}
