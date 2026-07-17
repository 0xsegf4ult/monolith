#include <cpu.h>
#include <irq.h>
#include <idt.h>
#include <pic.h>
#include <serial.h>

#include <klog.h>
#include <panic.h>
#include <sys/smp.h>

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

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void parse_bootloader_info()
{

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

	panic("could not start scheduler");
}
