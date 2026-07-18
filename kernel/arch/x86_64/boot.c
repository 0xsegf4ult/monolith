#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/ioapic.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/lapic.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/serial.h>

#include <mm/memory_map.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <init.h>
#include <klog.h>
#include <panic.h>
#include <sys/smp.h>

#include <sys/timer.h>

#define LIMINE_API_REVISION 3
#include <limine.h>

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request ss_request =
{
	.id = LIMINE_STACK_SIZE_REQUEST,
	.stack_size = 4096
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request =
{
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request kaddr_request =
{
	.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request =
{
	.id = LIMINE_RSDP_REQUEST
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void parse_bootloader_info()
{	
	if(memmap_request.response == nullptr)
		panic("EFI memory map invalid");

	parse_memory_map(&boot_info.memory_map, (virtaddr_t)memmap_request.response->entries, memmap_request.response->entry_count);

	if(kaddr_request.response == nullptr)
		panic("kernel load address invalid");

	boot_info.kload_addr = kaddr_request.response->physical_base;

	if(rsdp_request.response == nullptr)
		panic("EFI RSDP address invalid");

	boot_info.rsdp_address = (virtaddr_t)(rsdp_request.response->address) + VM_DMAP_BASE;
}

void init()
{
	local_irq_disable();
	pic_disable();

	early_serial_init();
	klog_init();

	klog("monolith kernel version git-\n");	
	parse_bootloader_info();
	idt_setup();
	smp_start_bsp();

	pmm_init(&boot_info.memory_map);
	slab_init();
	vmm_init_kpages(&boot_info.memory_map, boot_info.kload_addr);

	acpi_parse_rsdp((const rsdp_v1*)boot_info.rsdp_address);	

	ioapic_init();
	pit_init();
	lapic_init();

	panic("could not start scheduler");
}
