#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/pic.hpp>
#include <arch/x86_64/serial.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/timer.hpp>

#include <dev/efifb.hpp>
#include <dev/pcie.hpp>
#include <dev/ps2.hpp>
#include <dev/pseudo.hpp>
#include <dev/tty.hpp>
#include <dev/rtc.hpp>


#include <fs/procfs/procfs.hpp>
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
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <sys/time.hpp>

#include <sys/spinlock.hpp>

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
	asm volatile("cli; hlt");
}

extern "C" void init()
{
	for(ctor_func_t* ctor = start_ctors; ctor < end_ctors; ctor++)
	{
		(*ctor)();
	}

	early_serial_init();
	klog_init();
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

	size_t max_fb = 0;
	efifb_framebuffer framebuffer;
	if(framebuffer_request.response)
	{
		for(size_t i = 0; i < framebuffer_request.response->framebuffer_count; i++)
		{
			auto* fb = framebuffer_request.response->framebuffers[i];
			auto fsize = fb->width * fb->height * fb->bpp;
			if(fsize > max_fb)
			{
				max_fb = fsize;
				framebuffer.address = (virtaddr_t)fb->address - mm::direct_mapping_offset;
				framebuffer.width = fb->width;
				framebuffer.height = fb->height;
				framebuffer.pitch = fb->pitch;
				framebuffer.bpp = fb->bpp;
			}
		}
	}

	smp_start_bsp();

	pic::disable();

	pmm_initialize(memmap);
	mm::slab_init();
	vmm_init_kpages(memmap, phys_kernel_start);
	vfs::init();
	vfs::mkdir("/dev", 0755);

	auto acpi_tables = acpi::parse_tables(rsdp);
	
	pit::init(1193);
	log::info("timer: initialized source PIT 1ms period");
	
	if(acpi_tables.fadt->boot_architecture_flags & 0x2)
		ps2::init();

	efifb_init(framebuffer);

	smp_init();

	panic("idle thread died");
}

void kernel_main()
{
	pseudo_init();

	rtc_init();
	auto time = rtc_read();
	time_set_boottime(time);
	log::info("rtc: updated system time to {}", time);

	vfs::mkdir("proc", 0755);
	auto* procfs = procfs_create();
	vfs::mount("/proc", procfs);

	vfs::mkdir("sys", 0755);

	vmm_late_init();

	tty_init();
	
	pcie::enumerate();

	log::info("initramfs [{:#x} - {:#x}]", initramfs_address, reinterpret_cast<virtaddr_t>(initramfs_address) + initramfs_size);
	mmu_map_range(get_kernel_vmspace()->root_pml4, (physaddr_t)initramfs_address - mm::direct_mapping_offset, (virtaddr_t)initramfs_address, initramfs_size, vm_flags_to_x86(vm_present));
	initramfs_unpack(initramfs_address, initramfs_size);

	const char* argv[2] = {"/bin/init", nullptr};
	auto* init_proc = create_thread("init", argv, true);
	int init_status = load_executable("/bin/init", init_proc, nullptr);
	if(init_status < 0)
		panic("could not execute /bin/init!");
	sched_add_ready(init_proc);
}
