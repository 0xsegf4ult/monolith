#include <lib/klog.hpp>
#include <arch/x86_64/serial.hpp>

void klog_internal(const char* buffer)
{
	early_serial_write(buffer);
}

void klog_internal_newline()
{
	early_serial_putchar('\n');
}
