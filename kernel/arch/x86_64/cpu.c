#include <arch/x86_64/cpu.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/idt.h>

#include <sys/smp.h>
#include <config.h>
#include <klog.h>
#include <panic.h>
#include <types.h>

#include <cpuid.h>

static cpu_t cpus[CONFIG_MAX_CPUS];
static size_t cpu_count = 1;

cpu_t* smp_get_cpu(uint32_t id)
{
	return &cpus[id];
}

void smp_start_bsp()
{
	local_irq_disable();

	cpu_t* bsp = cpus;
	bsp->id = 0;
	bsp->lapic_id = 0;

	gdt_init(bsp);
	idt_load();

	wrmsr(MSR_GS_BASE, (virtaddr_t)bsp);

	klog("x86: PAT configured: WB WC UC- UC WB WP UC- WT\n");
	wrmsr(MSR_IA32_PAT, 0x0407050600070106);

	uint32_t rax, rbx, rcx, rdx;
	__get_cpuid(1u, &rax, &rbx, &rcx, &rdx);
	if(!(rcx & (1 << 26)))
		panic("cpu0: XSAVE not supported");
}
