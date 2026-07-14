#include <sys/timer.hpp>
#include <sys/scheduler.hpp>

void timer_interrupt()
{
	schedule();
}
