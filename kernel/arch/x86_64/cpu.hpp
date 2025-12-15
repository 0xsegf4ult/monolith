#pragma once

#include <lib/types.hpp>

class CPU
{
public:
	CPU() = default;

	void early_init(uint32_t cpu)
	{
		disable_interrupts();
		self = this;
		id = cpu;
	}

	static void disable_interrupts()
	{
		asm volatile("cli");
	}

	static void enable_interrupts()
	{
		asm volatile("sti");
	}

	[[noreturn]] static void halt()
	{
		for(;;)
		{
			asm volatile("cli; hlt");
		}
	}
private:
	CPU* self;
	uint32_t id;
};
