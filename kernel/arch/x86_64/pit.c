#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/ioapic.h>
#include <arch/x86_64/io.h>
#include <sys/clock.h>
#include <sys/irq.h>
#include <klog.h>
#include <types.h>

static uint64_t ticks = 0;
constexpr hwirq_t ISA_IRQ_PIT = 0;

void irq_handler(void* payload)
{
	ticks++;
}

static uint64_t pit_read(clocksource_t* source)
{
	return ticks * 1000000;
}

static void pit_enable(clocksource_t* source)
{
	irq_register(ISA_IRQ_PIT, irq_handler, nullptr);
	
	outb(0b00110100, 0x43);
	outb(1193 & 0xFF, 0x40);
	outb((1193 >> 8) & 0xFF, 0x40);
}

static void pit_disable(clocksource_t* source)
{

}

static clocksource_t pit_cs =
{
	.name = "pit",
	.priority = 10,
	.read = pit_read,
	.enable = pit_enable,
	.disable = pit_disable,
	.list_node = {&pit_cs.list_node, &pit_cs.list_node}
};

void pit_init()
{
	clocksource_register(&pit_cs);
}
