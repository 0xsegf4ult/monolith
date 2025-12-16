#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/pic.hpp>
#include <arch/x86_64/serial.hpp>

#include <mm/memory_map.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>

#define LIMINE_API_REVISION 3
#include <limine.h>

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile limine_memmap_request memmap_request =
{
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile limine_stack_size_request ss_request =
{
        .id = LIMINE_STACK_SIZE_REQUEST,
        .stack_size = 8192
};

__attribute__((used, section(".limine_requests")))
static volatile limine_rsdp_request rsdp_request =
{
        .id = LIMINE_RSDP_REQUEST
};

__attribute__((used, section(".limine_requests")))
static volatile limine_executable_address_request kaddr_request =
{
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST
};

__attribute__((used, section(".limine_requests")))
static volatile limine_module_request module_request =
{
        .id = LIMINE_MODULE_REQUEST
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern "C" char* virt_kernel_start;
extern "C" char* virt_kernel_end;

typedef void (*ctor_func_t)();
extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];

extern "C" void __assertion_fail_handler(const char* str)
{
        early_serial_write("\nassertion failed: ");
        early_serial_write(str);
        early_serial_putchar('\n');
        CPU::halt();
}

extern "C" [[noreturn]] void init()
{
	CPU bootCPU;

	for(ctor_func_t* ctor = start_ctors; ctor < end_ctors; ctor++)
		(*ctor)();

	early_serial_init();
	log::info("monolith kernel version git-");


	log::info("kernel virt memory [{} - {}]", &virt_kernel_start, &virt_kernel_end);
	if(memmap_request.response == nullptr)
		panic("EFI memory map pointer invalid");

	auto memmap = mm::parse_memmap(memmap_request.response->entries, memmap_request.response->entry_count);

	if(kaddr_request.response == nullptr)
		panic("bootloader supplied kernel physical load address invalid");
	
	physaddr_t phys_kernel_start = kaddr_request.response->physical_base;

	if(rsdp_request.response == nullptr)
		panic("EFI RSDP pointer invalid");

	auto* rsdp = reinterpret_cast<const acpi::rsdp_v1*>(reinterpret_cast<byte*>(rsdp_request.response->address) + mm::direct_mapping_offset);

	bootCPU.early_init(0);

	pmm_initialize(memmap);
	mm::slab_init();
	vmm_init_kpages(memmap, phys_kernel_start);

	auto acpi_tables = acpi::parse_tables(rsdp);

	pic::disable();
	lapic::enable(acpi_tables.madt->lapic_address);

	for(;;)
		asm volatile("hlt");
}
