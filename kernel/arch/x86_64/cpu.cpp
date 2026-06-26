#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <types.hpp>
#include <klog.hpp>
#include <sys/thread.hpp>

void cpu_t::set_pagetable(page_table* pt_address)
{
	pt = pt_address;
	asm volatile("movq %0, %%cr3" : : "r"(reinterpret_cast<uint64_t>(pt_address) - mm::direct_mapping_offset));
}

void cpu_t::set_current_thread(thread_t* thr)
{
	tss.rsp0 = thr->rsp0_top;
	current_thread = thr;
}

extern "C" void cpu_switch_thread(thread_t* prev, thread_t* next)
{
	smp_current_cpu()->set_current_thread(next);
	next->vm_space->switch_to();
}
