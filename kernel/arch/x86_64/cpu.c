#include <arch/x86_64/cpu.h>
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/lapic.h>

#include <mm/slab.h>
#include <mm/vmm.h>

#include <libk/string.h>

#include <sched/scheduler.h>
#include <sched/task.h>
#include <sys/smp.h>
#include <config.h>
#include <klog.h>
#include <panic.h>
#include <types.h>

#include <cpuid.h>

static cpu_t cpus[CONFIG_MAX_CPUS];
static size_t cpu_count = 1;
alignas(64) static uint8_t default_xsave[512];

extern void enable_sse();
extern void enable_xsave();

extern char ap_trampoline_start;

constexpr physaddr_t trampoline_start 	= 0x1000;
constexpr physaddr_t trampoline_cr3 	= 0x1ff0;
constexpr physaddr_t trampoline_jmp	= 0x1fe0;
constexpr physaddr_t trampoline_rsp	= 0x1fd0;

static _Atomic uint32_t running_cpu_count = 1;

cpu_t* smp_get_cpu(uint32_t id)
{
	return &cpus[id];
}

uint32_t smp_current_cpu()
{
	uint64_t value;
	asm volatile("movq %%gs:0, %[val]" : [val] "=r"(value));
	return value; 
}

struct task* smp_current_task()
{
	struct task* value;
	asm volatile("movq %%gs:16, %[val]" : [val] "=r"(value));
	return value;
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

	enable_sse();
	enable_xsave();

	asm volatile("fninit" ::: "memory");
	asm volatile("fxsave (%0)" :: "r"(default_xsave) : "memory");
}

void smp_start_ap(uint32_t lapic_id)
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

	gdt_init(cpu);
	idt_load();

	wrmsr(MSR_GS_BASE, (virtaddr_t)cpu);
	wrmsr(MSR_IA32_PAT, 0x0407050600070106);

	enable_sse();
	enable_xsave();
	asm volatile("fninit" ::: "memory");

	lapic_enable();

	atomic_fetch_add_explicit(&running_cpu_count, 1, memory_order_release);

	sched_start_cpu();
}

static void smp_discover_cpus()
{
	const madt* tbl = acpi_get_tables()->madt;
	const byte* raw = (const byte*)tbl;
	raw += sizeof(madt);

	size_t len = tbl->header.length - sizeof(madt);
	while(len)
	{
		const madt_entry* entry = (const madt_entry*)raw;

		if(entry->type == MADT_LAPIC)
		{
			const madt_lapic_entry* data = (madt_lapic_entry*)entry;

			if(data->apic_id)
			{
				if(cpu_count >= CONFIG_MAX_CPUS)
					panic("cpu_count >= CONFIG_MAX_CPUS");

				cpu_t* cpu = &cpus[cpu_count];
				cpu->id = cpu_count;
				cpu->lapic_id = data->apic_id;
				cpu_count++;
			}
		}

		raw += entry->length;
		len -= entry->length;
	}
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
		klog("smp: %s\n", cpuname);
	}

	smp_discover_cpus();

	klog("smp: bringing up %d CPUs\n", cpu_count);

	struct page_table* mmu_root = vm_get_kernel_space()->mmu_root;
	mmu_map(mmu_root, trampoline_start, trampoline_start, PROT_READ | PROT_WRITE | PROT_EXEC, 0);

	void (*ap_boot_fn)(uint32_t) = smp_start_ap;

	memcpy((void*)trampoline_start, &ap_trampoline_start, 0x1000);
	memcpy((void*)trampoline_cr3, &mmu_root, sizeof(void*));
	memcpy((void*)trampoline_jmp, &ap_boot_fn, sizeof(void*));

	for(int i = 1; i < cpu_count; i++)
	{
		auto lid = smp_get_cpu(i)->lapic_id;
		auto alloc = vmalloc(0x1000) + 0x1000;
		memcpy((void*)(trampoline_rsp - lid * 8), &alloc, sizeof(void*));
	}

	sched_init(cpu_count);

	for(int i = 1; i < cpu_count; i++)
	{
		lapic_send_ipi(smp_get_cpu(i)->lapic_id, LAPIC_INIT_IPI | LAPIC_ICR_INIT_DEASSERT);
		lapic_send_ipi(smp_get_cpu(i)->lapic_id, LAPIC_STARTUP_IPI | LAPIC_ICR_INIT_DEASSERT | (trampoline_start >> 12));	
	}

	while(atomic_load_explicit(&running_cpu_count, memory_order_acquire) < cpu_count)
		native_cpu_relax();

	mmu_unmap(mmu_root, trampoline_start);

	sched_start_cpu();
}

void smp_stop_cpus()
{
	uint32_t self = smp_get_cpu(smp_current_cpu())->lapic_id;
	for(int i = 0; i < cpu_count; i++)
	{
		auto lid = smp_get_cpu(i)->lapic_id;
		if(lid == self)
			continue;

		lapic_send_ipi(lid, LAPIC_NMI_IPI | LAPIC_ICR_INIT_DEASSERT);
	}
}

void cpu_set_pagetable(struct page_table* pgt)
{
	cpu_t* cpu = smp_get_cpu(smp_current_cpu());
	cpu->cur_pgt = pgt;
	asm volatile("movq %0, %%cr3" :: "r"((virtaddr_t)pgt - VM_DMAP_BASE) : "memory");
}

void cpu_switch_task(struct task* prev, struct task* next)
{
	cpu_t* cpu = smp_get_cpu(smp_current_cpu());
	cpu->tss.rsp0 = next->rsp0_top;
	cpu->current_task = next;

	if(prev->context)
		cpu_context_save(prev->context);

	native_set_tls(next->tls_base);

	if(prev->current_vm_space != next->current_vm_space)
		cpu_set_pagetable(next->current_vm_space->mmu_root);

	if(next->context)
		cpu_context_restore(next->context);
}

void native_set_tls(virtaddr_t base)
{
	wrmsr(MSR_FS_BASE, base);
}

struct cpu_context* cpu_context_new()
{
	struct cpu_context* ctx = (struct cpu_context*)kmalloc(sizeof(struct cpu_context));
	memcpy(ctx->simd, &default_xsave, 512);
	return ctx;
}

void cpu_context_destroy(struct cpu_context* ctx)
{
	kfree(ctx);
}

void cpu_context_save(struct cpu_context* ctx)
{
	asm volatile("fxsave (%0)" :: "r"(ctx->simd) : "memory");
}

void cpu_context_restore(struct cpu_context* ctx)
{
	asm volatile("fxrstor (%0)" :: "r"(ctx->simd) : "memory");
}

void dump_registers(interrupt_frame* frame)
{
        klog_nolock("RAX: %x RBX: %x RCX: %x\n", frame->rax, frame->rbx, frame->rcx);
        klog_nolock("RDX: %x RSI: %x RDI: %x\n",  frame->rdx, frame->rsi, frame->rdi);
        klog_nolock("RBP: %x R08: %x R09: %x\n", frame->rbp, frame->r8, frame->r9);
        klog_nolock("R10: %x R11: %x R12: %x\n", frame->r10, frame->r11, frame->r12);
        klog_nolock("R13: %x R14: %x R15: %x\n", frame->r13, frame->r14, frame->r15);
        klog_nolock("RIP: %x:%x\nRSP: %x:%x\nRFLAGS: %x\n", frame->cs, frame->rip, frame->ss, frame->rsp, frame->rflags);
}

static bool valid_frame(virtaddr_t addr)
{
	struct task* task = smp_current_task();
	if(addr >= 0x7fffffffffff)
        {
                if(addr + 8 >= task->rsp0)
                        return false;
        }
        else
                if(addr + 8 >= task->rsp)
                        return false;

        return true;
}

struct stack_frame
{
	struct stack_frame* rbp;
	virtaddr_t rip;
};

void stacktrace(virtaddr_t frame)
{
	struct stack_frame* stk = (struct stack_frame*)(frame > 0 ? (void*)frame : __builtin_frame_address(0));
	for(uint32_t sf = 0; stk && sf < 32; sf++)
	{
		if((virtaddr_t)stk & 0x7)
		{
			klog_nolock("<unaligned stackframe> %p ???\n", stk);
			break;
		}

		if(!valid_frame((virtaddr_t)stk))
		{
			klog_nolock("<reached top of stack> ???\n");
			break;
		}

		if(stk->rip == 0x0)
			break;

		klog_nolock("%p %s\n", stk->rip, "???");
		stk = stk->rbp;
	}
}
