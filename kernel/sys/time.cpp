#include <sys/time.hpp>
#include <types.hpp>
#include <stdatomic.h>

static time_t boot_time = 0;
static time_t uptime = 0;

void time_set_boottime(time_t time)
{
	boot_time = time;
}

void time_uptime_increment()
{
	atomic_fetch_add_explicit(reinterpret_cast<_Atomic(int64_t)*>(&uptime), 1, memory_order_relaxed);
}

time_t time_get_boottime()
{
	return boot_time;
}

time_t time_get_uptime()
{
	return atomic_load_explicit(reinterpret_cast<_Atomic(int64_t)*>(&uptime), memory_order_relaxed);
}

time_t time_get_current()
{
	return time_get_boottime() + time_get_uptime();
}
