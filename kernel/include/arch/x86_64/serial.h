#pragma once

#include <stdint.h>
#include <stddef.h>
#include <io.h>

enum UART_REG : uint16_t
{
	UART_DLL = 0,
	UART_IER = 1,
	UART_DLH = 1,
	UART_FCR = 2,
	UART_LCR = 3,
	UART_MCR = 4,
	UART_LSR = 5,
	COM1 = 0x3f8
};

static inline void early_serial_init()
{
	outb(0x00, COM1 + UART_IER);
	outb(0x80, COM1 + UART_LCR);
	outb(0x03, COM1 + UART_DLL);
	outb(0x00, COM1 + UART_DLH);
	outb(0x03, COM1 + UART_LCR);
	outb(0x07, COM1 + UART_FCR);
	outb(0x0b, COM1 + UART_MCR);
}

static inline uint8_t early_serial_tx_empty()
{
	return inb(COM1 + UART_LSR) & 0x20;
}

static inline void early_serial_putchar(char chr)
{
	while(!early_serial_tx_empty())
	{
		asm volatile("pause");
	}

	outb(chr, COM1);
}

static inline void early_serial_write(const char* string)
{
	size_t chr = 0;
	while(string[chr] != '\0')
	{
		early_serial_putchar(string[chr]);
		chr++;
	}
}
