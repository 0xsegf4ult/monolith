#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/ioapic.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/lapic.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/serial.h>

#include <dev/efifb.h>

#include <mm/memory_map.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <sys/smp.h>

#include <libk/string.h>

#include <init.h>
#include <klog.h>
#include <panic.h>

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request =
{
	.id = LIMINE_FRAMEBUFFER_REQUEST
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request =
{
	.id = LIMINE_MODULE_REQUEST
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void parse_framebuffer_info()
{
	size_t max_fb = 0;
	if(!framebuffer_request.response)
		return;

	for(size_t i = 0; i < framebuffer_request.response->framebuffer_count; i++)
	{
		struct limine_framebuffer* fb = framebuffer_request.response->framebuffers[i];
		size_t fsize = fb->width * fb->height * fb->bpp;
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

static void find_initramfs()
{
	if(!module_request.response || module_request.response->module_count < 1)
		panic("Could not find initramfs: no modules passed from bootloader");

	for(uint32_t i = 0; i < module_request.response->module_count; i++)
	{
		struct limine_file* mod = module_request.response->modules[i];
		if(strncmp(mod->string, "initramfs", 9) == 0)
		{
			boot_info.initramfs_address = (virtaddr_t)mod->address;
			boot_info.initramfs_size = mod->size;
			break;
		}
	}

	if(!boot_info.initramfs_address || !boot_info.initramfs_size)
		panic("failed to load initramfs");
}

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

	parse_framebuffer_info();
	find_initramfs();
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

	smp_init();
	panic("could not start scheduler");
}
