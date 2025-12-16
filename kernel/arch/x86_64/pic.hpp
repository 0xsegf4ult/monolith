#pragma once

#include <arch/x86_64/io.hpp>
#include <lib/types.hpp>

namespace pic
{

enum port : uint16_t
{
	command_master = 0x20,
	data_master = 0x21,
	command_slave = 0xa0,
	data_slave = 0xa1
};

constexpr uint8_t icw1 = 0x11;
constexpr uint8_t icw2_master = 0x20;
constexpr uint8_t icw2_slave = 0x28;
constexpr uint8_t icw3_master = 0x2;
constexpr uint8_t icw3_slave = 0x4;
constexpr uint8_t icw4_8086 = 0x1;

inline void disable()
{
	io::outb(icw1, port::command_master);
	io::outb(icw1, port::command_slave);
	io::outb(icw2_master, port::data_master);
	io::outb(icw2_slave, port::data_slave);
	io::outb(icw3_master, port::data_master);
	io::outb(icw3_slave, port::data_slave);
	io::outb(icw4_8086, port::data_master);
	io::outb(icw4_8086, port::data_slave);

	io::outb(0xFF, port::data_master);
	io::outb(0xFF, port::data_slave);
}

}
