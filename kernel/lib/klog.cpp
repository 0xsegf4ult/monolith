#include <lib/klog.hpp>
#include <arch/x86_64/serial.hpp>
#include <sys/spinlock.hpp>

static spinlock_t klog_lock;

void klog_init()
{
	spinlock_init(klog_lock);
}

void klog_internal(const char* buffer)
{
	spinlock_acquire(klog_lock);
	early_serial_write(buffer);
	spinlock_release(klog_lock);
}

void klog_internal(const char* buffer, size_t length)
{
	spinlock_acquire(klog_lock);
	for(size_t i = 0; i < length; i++)
		early_serial_putchar(buffer[i]);
	spinlock_release(klog_lock);
}

void klog_internal_newline()
{
	spinlock_acquire(klog_lock);
	early_serial_putchar('\n');
	spinlock_release(klog_lock);
}
