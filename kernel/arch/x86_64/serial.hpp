#pragma once

#include <lib/types.hpp>

namespace serial
{

enum UART
{
	DLL = 0,
	IER = 1,
	DLH = 1,
	FCR = 2,
	LCR = 3,
	MCR = 4,
	LSR = 5
};

constexpr uint16_t COM1 = 0x3f8;

}

void early_serial_init();
uint8_t early_serial_tx_empty();
void early_serial_putchar(char chr);
void early_serial_write(const char* string);
