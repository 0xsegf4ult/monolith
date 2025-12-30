#include <dev/ps2.hpp>

#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <sys/scheduler.hpp>

namespace ps2
{

struct key_event
{
	uint8_t code;
};

constexpr size_t kbd_buffer_size = 256;
key_event kbd_buffer[kbd_buffer_size];
uint8_t kbd_buf_position = 0;

void interrupt_handler()
{
	auto scancode = io::inb(0x60);

	kbd_buffer[kbd_buf_position].code = scancode;
	kbd_buf_position = (kbd_buf_position + 1) % kbd_buffer_size;

	if(scancode == 0x1f)
	{
		log::debug("schedule()");
		lapic::eoi();
		schedule();
	}
	else if(scancode == 0x20)
	{
		log::debug("sched_dump_state()");
		sched_dump_state();
	}
}

void init()
{
	log::info("ps2: initializing controller");

	io::outb(0xad, 0x64);
	io::outb(0xa7, 0x64);

	io::outb(0x20, 0x64);
	uint8_t config = io::inb(0x60);

	while(io::inb(0x64) & 1)
	{
		auto code = io::inb(0x60);
	}

	io::outb(0xae, 0x64);
	
	io::outb(0xff, 0x60);
	auto initcode = io::inb(0x60);
	initcode = io::inb(0x60);

	log::info("ps2: detected keyboard");

	ioapic::get(0).write_redirection_entry(0x1, 0x20);
	install_irq_handler(0x20, interrupt_handler);
}

}
