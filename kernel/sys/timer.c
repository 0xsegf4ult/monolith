#include <sys/timer.h>
#include <sched/scheduler.h>
#include <libk/list.h>
#include <klog.h>
#include <panic.h>
#include <types.h>

static list_head_t timer_devices = {&timer_devices, &timer_devices};
static timer_device* current = nullptr;
static uint8_t max_prio = 0;

void timer_interrupt()
{
	current->eoi();
	schedule();
}

void timer_start()
{
	if(!current)
		panic("no timer devices configured!");

	current->set_periodic(current);
}

void timer_device_register(timer_device* device)
{
	list_add_tail(&timer_devices, &device->list_node);
	klog("timer: %s\n", device->name);
	if(device->priority > max_prio)
	{
		max_prio = device->priority;
		current = device;
	}
}
