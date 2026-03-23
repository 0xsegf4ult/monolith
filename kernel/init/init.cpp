#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/pic.hpp>
#include <arch/x86_64/serial.hpp>
#include <arch/x86_64/timer.hpp>

#include <dev/pcie.hpp>
#include <dev/ps2.hpp>
#include <dev/tty.hpp>

#include <fs/vfs.hpp>

#include <init/initramfs.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/memory_map.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/executable.hpp>
#include <sys/process.hpp>
#include <sys/scheduler.hpp>

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
	
static byte* initramfs_address{nullptr};
static size_t initramfs_size{0};

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
	{
		(*ctor)();
	}

	early_serial_init();
	log::info("monolith kernel version git-");
	
	log::info("kernel virt memory [{:#x} - {:#x}]", &virt_kernel_start, &virt_kernel_end);
	if(memmap_request.response == nullptr)
		panic("EFI memory map pointer invalid");

	auto memmap = mm::parse_memmap(memmap_request.response->entries, memmap_request.response->entry_count);

	if(kaddr_request.response == nullptr)
		panic("bootloader supplied kernel physical load address invalid");
	
	physaddr_t phys_kernel_start = kaddr_request.response->physical_base;

	if(module_request.response == nullptr || module_request.response->module_count < 1)
		panic("failed to load initramfs");


	for(uint32_t i = 0; i < module_request.response->module_count; i++)
	{
		limine_file* mod = module_request.response->modules[i];
		if(!strncmp(mod->string, "initramfs", 9))
		{
			initramfs_address = reinterpret_cast<byte*>(mod->address);
			initramfs_size = mod->size;
		}
	}

	if(!initramfs_address || !initramfs_size)
		panic("failed to load initramfs");

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
	
	pit::init(1193);
	log::info("timer: initialized source PIT 1ms period");
	
	if(acpi_tables.fadt->boot_architecture_flags & 0x2)
		ps2::init();

	vfs::init();
	sched_start();

	panic("idle process died");
}

void kernel_main()
{
	vfs::mkdir("dev");	
	vfs::mkdir("proc");
	vfs::mkdir("sys");

	tty_init();
	
	pcie::enumerate();
	
	log::info("initramfs [{:#x} - {:#x}]", initramfs_address, reinterpret_cast<virtaddr_t>(initramfs_address) + initramfs_size);
	vm_map_range(reinterpret_cast<physaddr_t>(initramfs_address) - mm::direct_mapping_offset, reinterpret_cast<virtaddr_t>(initramfs_address), initramfs_size);
	initramfs_unpack(initramfs_address, initramfs_size);

	auto init_f = vfs::open("/bin/init");
	if(init_f < 0)
		panic("could not find /bin/init");

	const char* argv[2] = {"init", nullptr};
	auto* init_proc = create_process("init", argv, true);
	load_executable(init_f, init_proc);
	vfs::close(init_f);
	sched_add_ready(init_proc);
}
