#include <klog.h>
#include <libk/vsprintf.h>
#include <sys/spinlock.h>
#include <serial.h>

static spinlock_t lock;
static char log_buffer[1024];

void klog_init()
{
	spinlock_init(&lock);
}

void klog_write_nolock(const char* string)
{
	early_serial_write(string);
}

void klog_nolock(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ssize_t written = vsprintf(log_buffer, fmt, args);
	va_end(args);

	log_buffer[written] = '\0';
	klog_write_nolock(log_buffer);
}

void klog(const char* fmt, ...)
{
	uint64_t irqflags;
	spinlock_acquire_irqsave(&lock, &irqflags);

	va_list args;
	va_start(args, fmt);
	ssize_t written = vsprintf(log_buffer, fmt, args);
	va_end(args);

	log_buffer[written] = '\0';
	klog_write_nolock(log_buffer);

	spinlock_release_irqsave(&lock, irqflags);
}
