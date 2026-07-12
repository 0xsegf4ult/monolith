#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <sys/task.hpp>
#include <types.hpp>
#include <klog.hpp>

void cpu_t::set_pagetable(page_table* pt_address)
{
	pt = pt_address;
	asm volatile("movq %0, %%cr3" : : "r"(reinterpret_cast<uint64_t>(pt_address) - mm::direct_mapping_offset));
}

void cpu_t::set_current_task(task_t* task)
{
	tss.rsp0 = task->rsp0_top;
	current_task = task;
}

extern "C" void cpu_switch_task(task_t* prev, task_t* next)
{
	smp_current_cpu()->set_current_task(next);
	next->current_vm_space->switch_to();
}
