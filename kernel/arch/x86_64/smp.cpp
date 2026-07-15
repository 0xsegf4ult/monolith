#include <sys/smp.hpp>

#include <arch/generic.hpp>

#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/lapic.hpp>
#include <arch/x86_64/mmu.hpp>

#include <types.hpp>
#include <klog.hpp>
#include <kstd.hpp> 
#include <panic.hpp>

#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/scheduler.hpp>
#include <sys/task.hpp>
#include <cpuid.h>
#include <stdatomic.h>

static cpu_t cpus[CONFIG_MAX_CPUS];
static size_t cpu_count = 1;
extern "C" void enable_sse();
extern "C" void enable_xsave();
extern "C" char ap_trampoline_start; 
alignas(64) static uint8_t default_xsave[512];

constexpr physaddr_t trampoline_start = 0x1000;
constexpr physaddr_t trampoline_cr3 = 0x1ff0;
constexpr physaddr_t trampoline_jmp = 0x1fe0;
constexpr physaddr_t trampoline_rsp = 0x1fd0;

atomic_uint running_cpu_count = 1; 

static cpu_t* smp_get_cpu(uint32_t id)
{
	return &cpus[id];
}

static void smp_discover_cpu(uint32_t lapic_id)
{
	if(cpu_count >= CONFIG_MAX_CPUS)
		panic("cpu_count >= CONFIG_MAX_CPUS");

	cpu_t* cpu = &cpus[cpu_count];
	cpu->id = cpu_count;
	cpu->lapic_id = lapic_id;
	cpu_count++;
}

static void smp_discover_cpus()
{
	auto* madt = acpi_get_tables().madt;
	auto* raw = (const byte*)madt;
	raw += sizeof(acpi::madt);

	auto len = madt->length - sizeof(acpi::madt);
	while(len)
	{
		auto* entry = (const acpi::madt_entry*)raw;

		if(entry->entry_type == acpi::MADTEntryType::LAPIC)
		{
			auto* data = (const acpi::madt_lapic_entry*)entry;
			if(data->apic_id)
				smp_discover_cpu(data->apic_id);
		}
		
		raw += entry->entry_length;
	       	len -= entry->entry_length;
	}	
}

void smp_start_bsp()
{
	arch_disable_interrupts();
	
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

	asm volatile("fninit");
	asm volatile("fxsave (%0)" :: "r"(default_xsave));
}

extern "C" void smp_start_ap(uint32_t lapic_id)
{
	cpu_t* cpu;
	for(uint32_t i = 0; i < cpu_count; i++)
	{
		if(cpus[i].lapic_id == lapic_id)
		{
			cpu = &cpus[i];
			break;
		}
	}

	setup_gdt(cpu);
	load_idt();	
	
	wrmsr(GS_BASE, reinterpret_cast<virtaddr_t>(cpu));

	wrmsr(IA32_PAT_MSR, 0x0407050600070106);
	enable_sse();
	enable_xsave();
	asm volatile("fninit");
		
	lapic_enable();

	running_cpu_count++;

	sched_start_ap();
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

	smp_discover_cpus();

	log::info("smp: bringing up {} CPUs", cpu_count);

	auto* pgt = smp_get_cpu(smp_current_cpu())->pt;
	mmu_map(pgt, trampoline_start, trampoline_start, PROT_READ | PROT_WRITE | PROT_EXEC, 0);
	
	void (*fptr)(uint32_t) = smp_start_ap;

	memcpy((void*)((virtaddr_t)(trampoline_start)), &ap_trampoline_start, 0x1000);
	memcpy((void*)((virtaddr_t)(trampoline_cr3)), &pgt, sizeof(void*));
	memcpy((void*)((virtaddr_t)(trampoline_jmp)), &fptr, sizeof(void*));

	for(int i = 1; i < cpu_count; i++)
	{
		auto lid = smp_get_cpu(i)->lapic_id;
		auto alloc = vmalloc(0x1000) + 0x1000;
		memcpy((void*)((virtaddr_t)(trampoline_rsp) - lid * 8), &alloc, sizeof(void*));
	}

	sched_init(cpu_count);

	for(int i = 1; i < cpu_count; i++)
	{
		lapic_send_ipi(smp_get_cpu(i)->lapic_id, LAPIC_INIT_IPI | LAPIC_ICR_INIT_DEASSERT);
		lapic_send_ipi(smp_get_cpu(i)->lapic_id, LAPIC_STARTUP_IPI | LAPIC_ICR_INIT_DEASSERT | (trampoline_start >> 12));
	}

	while(running_cpu_count < cpu_count)
	{
		arch_pause();
	}

	mmu_unmap(pgt, trampoline_start);

	sched_start_bsp();
}


uint32_t smp_current_cpu()
{
	uint64_t value;
	asm volatile("movq %%gs:0, %[val]" : [val] "=r"(value));
	return value;
}

task_t* smp_current_task()
{
	task_t* value;
	asm volatile("movq %%gs:16, %[val]" : [val] "=r"(value));
	return value;
}

void smp_stop_cpus()
{
	auto self = smp_get_cpu(smp_current_cpu())->lapic_id;
	for(int i = 0; i < cpu_count; i++)
	{
		auto lid = smp_get_cpu(i)->lapic_id;
		if(lid == self)
			continue;
		
		lapic_send_ipi(lid, LAPIC_NMI_IPI | LAPIC_ICR_INIT_DEASSERT);
	}
}

void smp_set_current_pagetable(page_table* table)
{
	auto* cpu = smp_get_cpu(smp_current_cpu());
	cpu->pt = table;
	asm volatile("movq %0, %%cr3" :: "r"(virtaddr_t(cpu->pt) - VM_DMAP_BASE));
}

arch_context_t* arch_context_new()
{
	auto* ctx = (arch_context_t*)kmalloc(sizeof(arch_context_t));
	memcpy(ctx->simd, &default_xsave, 512);
	return ctx;
}

void arch_context_destroy(arch_context_t* ctx)
{
	kfree(ctx);
}

void arch_context_save(arch_context_t* ctx)
{
	asm volatile("fxsave (%0)" :: "r"(ctx->simd));
}

void arch_context_restore(arch_context_t* ctx)
{
	asm volatile("fxrstor (%0)" :: "r"(ctx->simd));
}

extern "C" void cpu_switch_task(task_t* prev, task_t* next)
{
	auto* cpu = smp_get_cpu(smp_current_cpu());
	cpu->tss.rsp0 = next->rsp0_top;
	cpu->current_task = next;

	if(prev->context)
		arch_context_save(prev->context);

	arch_set_tls(next->tls_base);

	if(prev->current_vm_space != next->current_vm_space)
	{
		cpu->pt = next->current_vm_space->mmu_root;
		asm volatile("movq %0, %%cr3" :: "r"(virtaddr_t(cpu->pt) - VM_DMAP_BASE));
	}

	if(next->context)
		arch_context_restore(next->context);
}
