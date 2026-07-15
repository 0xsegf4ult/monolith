#include <arch/x86_64/pit.hpp>
#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/ioapic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/generic.hpp>

#include <sys/task.hpp>
#include <sys/time.hpp>
#include <types.hpp>

static uint64_t ticks = 0;
namespace timer
{
	uint64_t get_ticks()
	{
		return ticks;
	}
}

void irq_handler()
{
	ticks++;
	time_uptime_add(1000000);
}

void pit_init()
{
	auto irq = allocate_irq();
	ioapic_write_redirection_entry(0, irq);
	install_irq_handler(irq, irq_handler);
	
	io::outb(0b00110100, 0x43);
	io::outb(1193 & 0xFF, 0x40);
	io::outb((1193 >> 8) & 0xFF, 0x40);
}

void pit_sleep(uint32_t ms)
{
	auto target_ticks = ticks + ms;
	while(ticks < target_ticks)
	{
		arch_pause();
	}
}
