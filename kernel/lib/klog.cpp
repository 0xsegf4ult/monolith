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
	uint64_t rflags;
	spinlock_acquire_irqsave(klog_lock, rflags);
	early_serial_write(buffer);
	spinlock_release_irqsave(klog_lock, rflags);
}

void klog_internal(const char* buffer, size_t length)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(klog_lock, rflags);
	for(size_t i = 0; i < length; i++)
		early_serial_putchar(buffer[i]);
	spinlock_release_irqsave(klog_lock, rflags);
}

void klog_internal_newline()
{
	uint64_t rflags;
	spinlock_acquire_irqsave(klog_lock, rflags);
	early_serial_putchar('\n');
	spinlock_release_irqsave(klog_lock, rflags);
}
