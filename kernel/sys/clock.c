#include <sys/clock.h>
#include <libk/list.h>
#include <cpu.h>
#include <klog.h>
#include <types.h>

static list_head_t sources = {&sources, &sources};
static clocksource_t* current = nullptr;
static uint8_t max_prio = 0;

void clocksource_register(clocksource_t* source)
{
	list_add_tail(&sources, &source->list_node);
	klog("clocksource: %s\n", source->name);
	if(source->priority > max_prio)
	{
		if(current)
			current->disable(current);

		max_prio = source->priority;
		current = source;

		current->enable(current);
		klog("clocksource: switched to %s\n", source->name);
	}
}

uint64_t clock_uptime()
{
	if(!current)
		return 0;

	return current->read(current);
}

void clock_wait(uint64_t nanos)
{
	if(!nanos)
		return;

	uint64_t target = clock_uptime() + nanos;
	while(clock_uptime() < target)
	{
		native_cpu_relax();
	}
}
