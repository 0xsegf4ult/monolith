#include <arch/x86_64/timer.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <lib/types.hpp>

#include <sys/scheduler.hpp>

namespace timer
{

static uint64_t ticks = 0;
uint64_t get_ticks()
{
	return ticks;
}

}

namespace pit
{

static uint8_t gsi = 0x0;

void set_gsi(uint8_t p_gsi)
{
	gsi = p_gsi;
}

void irq_handler()
{
	timer::ticks++;
	if(timer::ticks % 10 == 0)
		schedule();
}

void init(uint16_t count)
{
	io::outb(0b00110100, 0x43);
	io::outb(count & 0xFF, 0x40);
	io::outb(count >> 8, 0x40);

	auto irq = allocate_irq();
	ioapic::get(0).write_redirection_entry(pit::gsi, irq);
	install_irq_handler(irq, irq_handler);
}

}
