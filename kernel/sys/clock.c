#include <sys/clock.h>
#include <mm/vmm.h>
#include <libk/list.h>
#include <cpu.h>
#include <errno.h>
#include <klog.h>
#include <types.h>

static list_head_t sources = {&sources, &sources};
static clocksource_t* current = nullptr;
static uint8_t max_prio = 0;
static time_t boot_time = 0;

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

void clock_set_boottime(time_t boottime)
{
	boot_time = boottime;
}

int sys_clock_gettime(clockid_t clock, struct timespec* tv)
{
	if(clock != CLOCK_REALTIME)
		return -EINVAL;

	if(!vm_validate_ptr(tv, sizeof(struct timespec)))
		return -EFAULT;

	uint64_t uptime = clock_uptime();

	tv->tv_sec = (time_t)(uptime / 1000000000) + boot_time;
	tv->tv_nsec = (int64_t)(uptime % 1000000000); 

	return 0;
}
