#include <arch/x86_64/serial.hpp>
#include <arch/x86_64/io.hpp>
#include <lib/types.hpp>

using namespace serial;

void early_serial_init()
{
	io::outb(0x00, COM1 + UART::IER);
	io::outb(0x80, COM1 + UART::LCR);
	io::outb(0x03, COM1 + UART::DLL);
	io::outb(0x00, COM1 + UART::DLH);
	io::outb(0x03, COM1 + UART::LCR);
	io::outb(0x07, COM1 + UART::FCR);
	io::outb(0x0b, COM1 + UART::MCR);
}

uint8_t early_serial_tx_empty()
{
	return io::inb(COM1 + UART::LSR) & 0x20;
}

void early_serial_putchar(char chr)
{
	while(!early_serial_tx_empty());

	io::outb(chr, COM1);
}

void early_serial_write(const char* string)
{
	size_t chr = 0;
	while(string[chr] != '\0')
	{
		early_serial_putchar(string[chr]);
		chr++;
	}
}
