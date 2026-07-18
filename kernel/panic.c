#include <panic.h>
#include <irq.h>
#include <cpu.h>

#include <libk/vsprintf.h>
#include <sys/smp.h>
#include <klog.h>

#include <stdarg.h>

void panic_prepare()
{
	local_irq_disable();

}

void panic_complete()
{
	native_cpu_halt();
}

static char panic_buf[1024];

void panic(const char* fmt, ...)
{
	panic_prepare();

	klog_write_nolock("\n\033[31mkernel panic:\033[0m ");
	
	va_list args;
	va_start(args, fmt);
	ssize_t written = vsprintf(panic_buf, fmt, args);
	va_end(args);

	panic_buf[written] = '\n';
	klog_write_nolock(panic_buf);

	panic_complete();
}
