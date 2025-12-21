#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/pic.hpp>
#include <arch/x86_64/serial.hpp>
#include <arch/x86_64/timer.hpp>

#include <dev/ps2.hpp>

#include <fs/vfs.hpp>

#include <init/initramfs.hpp>

#include <lib/elf.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/memory_map.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

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

	if(module_request.response == nullptr || module_request.response->module_count < 1)
		panic("failed to load initramfs");

	byte* initramfs_address{nullptr};
	size_t initramfs_size{0};

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

	sched_init();
	
	log::info("initramfs [{:x} - {:x}]", initramfs_address, reinterpret_cast<virtaddr_t>(initramfs_address) + initramfs_size);
	vm_map_range(reinterpret_cast<physaddr_t>(initramfs_address) - mm::direct_mapping_offset, reinterpret_cast<virtaddr_t>(initramfs_address), initramfs_size);
	initramfs_unpack(initramfs_address, initramfs_size);

	auto init_f = vfs::open("/bin/init");
	if(init_f < 0)
		log::error("could not find /bin/init");

	auto* ebuf = (Elf64_Ehdr*)kmalloc(sizeof(Elf64_Ehdr));
	vfs::read(init_f, (byte*)ebuf, sizeof(Elf64_Ehdr));
	if(!elf_validate(ebuf))
		log::error("/bin/init not a valid ELF executable");
	
	auto* init_proc = create_process("init", true);
	init_proc->entry = ebuf->e_entry;
	log::debug("create process init entry {:x}", ebuf->e_entry);
	auto* phdrs = (Elf64_Phdr*)kmalloc(ebuf->e_phentsize * ebuf->e_phnum);
	vfs::seek(init_f, ebuf->e_phoff);
	vfs::read(init_f, (byte*)phdrs, ebuf->e_phentsize * ebuf->e_phnum);
	auto* phdr = phdrs;
	for(int i = 0; i < ebuf->e_phnum; i++)
	{
		log::debug("elf phdr: type {} flags {:b} offset {:x}", phdr->p_type, phdr->p_flags, phdr->p_offset);
		if(phdr->p_type == PT_LOAD)
		{
			log::debug("LOAD {}{}{}", (phdr->p_flags & PF_R) ? 'R' : '-', (phdr->p_flags & PF_W) ? 'W' : '-', (phdr->p_flags & PF_X) ? 'X' : '-');

			uint64_t vmflags = vm_user;
			if(phdr->p_flags & PF_W)
				vmflags |= vm_write;
			if(phdr->p_flags & PF_X)
			{
				if(phdr->p_flags & PF_W)
					log::warn("section has W+X flags!!!");
				vmflags |= vm_exec;
			}

			uint64_t page_offset = phdr->p_vaddr % 0x1000;
			virtaddr_t aligned_vaddr = phdr->p_vaddr - page_offset;
			log::debug("vm_alloc {:x} size {:x}", aligned_vaddr, phdr->p_memsz + page_offset);
			init_proc->vm_space->alloc_placed(aligned_vaddr, phdr->p_memsz + page_offset, vmflags);
			
			virtaddr_t user_page = init_proc->vm_space->get_mapping(aligned_vaddr) + mm::direct_mapping_offset;
			log::debug("section mapped at {:x}", user_page);

			vfs::seek(init_f, phdr->p_offset);
			log::debug("copy {:x} bytes from EHDR + {:x}", phdr->p_filesz, phdr->p_offset);
			vfs::read(init_f, (byte*)user_page + page_offset, phdr->p_filesz);
		       	auto zerocount = phdr->p_memsz - phdr->p_filesz;
			if(zerocount)
			{
				log::debug("zeroing {:x} bytes at {:x}", zerocount, phdr->p_memsz);
				memset((void*)(user_page + page_offset + phdr->p_filesz), 0, zerocount); 
			}	
		}

		phdr++;
	}
	vfs::close(init_f);
	sched_add_ready(init_proc);

	sched_start();

	panic("idle process died");
}
