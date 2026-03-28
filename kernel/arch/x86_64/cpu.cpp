#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <lib/types.hpp>
#include <lib/klog.hpp>
#include <sys/process.hpp>

void cpu_t::set_pagetable(page_table* pt_address)
{
	pt = pt_address;
	asm volatile("movq %0, %%cr3" : : "r"(reinterpret_cast<uint64_t>(pt_address) - mm::direct_mapping_offset));
}

void cpu_t::set_current_process(process_t* process)
{
	tss.rsp0 = process->rsp0_top;
	current_process = process;
}

extern "C" void cpu_switch_process(process_t* prev, process_t* next)
{
	smp_current_cpu()->set_current_process(next);
	next->vm_space->switch_to();
}
