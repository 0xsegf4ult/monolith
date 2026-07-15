#include <sys/timer.hpp>
#include <sys/scheduler.hpp>
#include <list.hpp>
#include <klog.hpp>
#include <panic.hpp>

static list_head_t timer_devices = {&timer_devices, &timer_devices};
static timer_device* cur_device;
static uint8_t cur_prio = 0;

void timer_interrupt()
{
	schedule();
}

void timer_start()
{
	if(!cur_device)
		panic("no timer devices configured!");

	cur_device->set_periodic(cur_device);
}

void timer_device_register(timer_device& device)
{
	list_add_tail(timer_devices, device.list_node);
	log::info("timer: {} registered", device.name);
	if(device.priority > cur_prio)
	{
		cur_prio = device.priority;
		cur_device = &device;
	}
}
