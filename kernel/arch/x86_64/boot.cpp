#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/ioapic.hpp>
#include <arch/x86_64/lapic.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/pic.hpp>
#include <arch/x86_64/pit.hpp>
#include <arch/x86_64/serial.hpp>

#include <arch/generic.hpp>

#include <dev/efifb.hpp>

#include <mm/memory_map.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/task.hpp>
#include <sys/smp.hpp>

#include <init.hpp>
#include <klog.hpp>
#include <kstd.hpp>
#include <panic.hpp>

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
        .stack_size = 4096
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

__attribute__((used, section(".limine_requests")))
static volatile limine_framebuffer_request framebuffer_request =
{
	.id = LIMINE_FRAMEBUFFER_REQUEST
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

typedef void (*ctor_func_t)();
extern ctor_func_t start_ctors[];
extern ctor_func_t end_ctors[];
	
static void run_ctors()
{
	for(ctor_func_t* ctor = start_ctors; ctor < end_ctors; ctor++)
	{
		(*ctor)();
	}
}

extern "C" void __assertion_fail_handler(const char* str)
{
        panic("assertion failed: {}", str);
}

static void parse_bootloader_info()
{
	if(memmap_request.response == nullptr)
		panic("EFI memory map pointer invalid");

	boot_info.memmap = mm::parse_memmap(memmap_request.response->entries, memmap_request.response->entry_count);
	if(kaddr_request.response == nullptr)
		panic("bootloader supplied kernel physical load address invalid");
	
	boot_info.phys_kernel_start = kaddr_request.response->physical_base;

	if(module_request.response == nullptr || module_request.response->module_count < 1)
		panic("failed to load initramfs");

	for(uint32_t i = 0; i < module_request.response->module_count; i++)
	{
		limine_file* mod = module_request.response->modules[i];
		if(!strncmp(mod->string, "initramfs", 9))
		{
			boot_info.initramfs_address = (virtaddr_t)mod->address;
			boot_info.initramfs_size = mod->size;
		}
	}

	if(!boot_info.initramfs_address || !boot_info.initramfs_size)
		panic("failed to load initramfs");

	if(rsdp_request.response == nullptr)
		panic("EFI RSDP pointer invalid");

	boot_info.rsdp_address = (virtaddr_t)(rsdp_request.response->address) + VM_DMAP_BASE;

	size_t max_fb = 0;
	if(framebuffer_request.response)
	{
		for(size_t i = 0; i < framebuffer_request.response->framebuffer_count; i++)
		{
			auto* fb = framebuffer_request.response->framebuffers[i];
			auto fsize = fb->width * fb->height * fb->bpp;
			if(fsize > max_fb)
			{
				max_fb = fsize;
				boot_info.fb.address = (virtaddr_t)fb->address - VM_DMAP_BASE;
				boot_info.fb.width = fb->width;
				boot_info.fb.height = fb->height;
				boot_info.fb.pitch = fb->pitch;
				boot_info.fb.bpp = fb->bpp;
			}
		}
	}
}

extern "C" void init()
{
	arch_disable_interrupts();
	pic::disable();
	
	run_ctors();

	early_serial_init();
	klog_init();
	
	log::info("monolith kernel version git-");
	parse_bootloader_info();

	smp_start_bsp();

	pmm_initialize(boot_info.memmap);
	mm::slab_init();
	vmm_init_kpages(boot_info.memmap, boot_info.phys_kernel_start);

	acpi_parse_rsdp((const acpi::rsdp_v1*)boot_info.rsdp_address);	
	
	ioapic_init();
	pit_init();
	lapic_init();	
	
	task_init();
	smp_init();
}
