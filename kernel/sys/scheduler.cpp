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

static process_t boot_proc = {};

static process_t* idle_process = nullptr;
static process_t* ready_list_head = nullptr;
static process_t* ready_list_tail = nullptr;
static process_t* kill_list = nullptr;
static bool sched_locked = false;


void idle_process_entry(void* arg)
{
	log::info("sched: running on bsp");
	CPU::enable_interrupts();

	for(;;)
	{
		asm volatile("hlt");
	}
}

void sched_cleaner()
{
	while(kill_list)
	{
		log::debug("cleaner walk {:#x}", kill_list);
		auto last = kill_list;
		kill_list = kill_list->next;
		destroy_process(last);
	}
}

void schedule()
{
	while(sched_locked)
	{
	}

	sched_locked = true;

	sched_cleaner();	

	auto* current_process = CPU::get_current()->get_current_process();
	if(!ready_list_head && current_process->status == process_status::running)
		return;

	CPU::disable_interrupts();

	if(current_process->status == process_status::running)
	{
		current_process->status = process_status::ready;
		if(current_process != idle_process)
		{
			ready_list_tail->next = current_process;
			ready_list_tail = current_process;
		}
	}

	process_t* last = current_process;	

	process_t* proc = ready_list_head;
	if(!proc)
		proc = idle_process;
	else
	{
		ready_list_head = proc->next;
		proc->next = nullptr;
		if(ready_list_tail == proc)
			ready_list_tail = ready_list_head;
	}

	sched_locked = false;

	proc->status = process_status::running; 
	arch_context_switch(last, proc);
}

void sched_block(process_status status)
{
	auto* current_process = CPU::get_current()->get_current_process();
	current_process->status = status;
	if(!sched_locked) schedule();
}

void sched_unblock(process_t* proc)
{
	while(sched_locked)
	{
	}

	sched_locked = true;
	proc->status = process_status::ready;
	sched_add_ready(proc);
	sched_locked = false;
}

void sched_kill()
{
	auto* current_process = CPU::get_current()->get_current_process();
	
	current_process->next = kill_list;
	kill_list = current_process;
	sched_block(process_status::terminated);	
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
	idle_process->rsp0_top = idle_process->rsp0;
	
	idle_process->status = process_status::running;
	arch_context_switch(&boot_proc, idle_process);
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

void sched_dump_state()
{
	auto* current_process = CPU::get_current()->get_current_process();
	log::debug("SCHEDULER STATE: ");
	generic_log("current: {}", current_process->name);
	generic_log("ready_list");
	auto* r_cur = ready_list_head;
	while(r_cur)
	{
		generic_log(" -> {}", r_cur->name);
		r_cur = r_cur->next;
	}
	generic_log("kill_list");
	auto* k_cur = kill_list;
	while(k_cur)
	{
		generic_log(" -> {}", k_cur->name);
		k_cur = k_cur->next;
	}
}
