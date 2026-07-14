#include <sys/time.hpp>
#include <sys/task.hpp>
#include <types.hpp>
#include <stdatomic.h>

static time_t boot_time = 0;
static uint64_t uptime_nsec = 0;

void time_set_boottime(time_t time)
{
	boot_time = time;
}

void time_uptime_add(uint64_t nsec)
{
	atomic_fetch_add_explicit(reinterpret_cast<_Atomic(uint64_t)*>(&uptime_nsec), nsec, memory_order_relaxed);
	task_tick_sleepers(nsec);
}

time_t time_get_boottime()
{
	return boot_time;
}

timespec time_get_uptime()
{
	uint64_t nsecs = atomic_load_explicit(reinterpret_cast<_Atomic(uint64_t)*>(&uptime_nsec), memory_order_relaxed);
	return {time_t(nsecs / 1000000000), uint32_t(nsecs % 1000000000)};
}

timespec time_get_current()
{
	auto uptime = time_get_uptime();
	uptime.tv_sec += time_get_boottime();
	return uptime;
}
