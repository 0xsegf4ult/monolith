#include <arch/x86_64/timer.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <types.hpp>

#include <sys/scheduler.hpp>
#include <sys/time.hpp>

namespace timer
{

static uint64_t ticks = 0;
uint64_t get_ticks()
{
	return ticks;
}

void sleep(uint64_t num)
{
	auto target_ticks = ticks + num;
       	while(ticks < target_ticks)
	{
		asm volatile("pause");
	}	
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
	if(timer::ticks % 1000 == 0)
		time_uptime_increment();
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

static uint64_t apic_timer_interval = 0;
static uint64_t apic_timer_gsi = 0;

void apic_timer_handler()
{
	schedule();
}

void apic_timer_calibrate()
{
	lapic::write(APIC_REGISTER_TIMER_DIVIDER, 0x3);
	
	lapic::write(APIC_REGISTER_TIMER_INITCNT, 0xFFFFFFFF);
	asm volatile("sti");
	timer::sleep(10);
	asm volatile("cli");
	lapic::write(APIC_REGISTER_LVT_TIMER, 1 << 16); // MASKED

	auto ticks = 0xFFFFFFFF - lapic::read(APIC_REGISTER_TIMER_CURRCNT);
	apic_timer_interval = ticks;
}

void apic_timer_enable()
{
	if(!apic_timer_gsi)
	{
		apic_timer_gsi = allocate_irq();
		install_irq_handler(apic_timer_gsi, apic_timer_handler);
	}

	lapic::write(APIC_REGISTER_LVT_TIMER, apic_timer_gsi | 0x20000);
	lapic::write(APIC_REGISTER_TIMER_DIVIDER, 0x3);
	lapic::write(APIC_REGISTER_TIMER_INITCNT, apic_timer_interval);	
}
