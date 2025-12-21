#include <arch/x86_64/timer.hpp>
#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/io.hpp>

#include <lib/types.hpp>

namespace pit
{

void init(uint16_t count)
{
	io::outb(0b00110100, 0x43);
	io::outb(count & 0xFF, 0x40);
	io::outb(count >> 8, 0x40);

	ioapic::get(0).write_redirection_entry(0x2, 0x21);
}

}
