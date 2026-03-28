#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <lib/types.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp> 

#include <sys/scheduler.hpp>
#include <cpuid.h>

static cpu_t cpus[arch_max_cpus];
static size_t cpu_count = 1;
extern "C" void enable_sse();
extern "C" void enable_xsave();

void smp_start_bsp()
{
	disable_interrupts();
	
	cpu_t* bsp = cpus;
	bsp->id = 0;
	bsp->lapic_id = 0;

	setup_gdt(bsp);
	setup_idt();

	wrmsr(GS_BASE, reinterpret_cast<virtaddr_t>(bsp));

	log::info("x86: PAT configured: WB WC UC- UC WB WP UC- WT");
	wrmsr(IA32_PAT_MSR, 0x0407050600070106);

	uint32_t c0, c1, c2, c3;
	__get_cpuid(1u, &c0, &c1, &c2, &c3);
	if(!(c2 & (1 << 26)))
		panic("cpu0: XSAVE not supported");

	enable_sse();
	enable_xsave();
}

void smp_init()
{
	uint32_t name_regs[12];
	char cpuname[12 * 4 + 1];
	__get_cpuid(0x80000000, &name_regs[0], &name_regs[1], &name_regs[2], &name_regs[3]);
	if(name_regs[0] >= 0x80000004)
	{
		__get_cpuid(0x80000002, &name_regs[0], &name_regs[1], &name_regs[2], &name_regs[3]);
		__get_cpuid(0x80000003, &name_regs[4], &name_regs[5], &name_regs[6], &name_regs[7]);
		__get_cpuid(0x80000004, &name_regs[8], &name_regs[9], &name_regs[10], &name_regs[11]);

		memcpy(cpuname, name_regs, 12 * 4);
		cpuname[12 * 4] = '\0';
		log::info("smp: {}", cpuname);
	}
	
	log::info("smp: bringing up {} CPUs", cpu_count);
	lapic::enable();
	sched_start();
}

void smp_discover_cpu(uint32_t lapic_id)
{
	if(cpu_count == arch_max_cpus)
		panic("cpu_count >= arch_max_cpus");

	cpu_t* cpu = &cpus[cpu_count];
	cpu->id = cpu_count;
	cpu->lapic_id = lapic_id;
	cpu_count++;
}

cpu_t* smp_get_cpu(uint32_t id)
{
	return &cpus[id];
}

cpu_t* smp_current_cpu()
{
	uint64_t value;
	asm volatile("mov %%gs:0, %[val]" : [val] "=r"(value));
	return smp_get_cpu(value);
}
