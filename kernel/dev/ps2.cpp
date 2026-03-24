#include <dev/ps2.hpp>
#include <dev/tty.hpp>

#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <sys/scheduler.hpp>

namespace ps2
{

enum kbd_modifier_key : uint32_t
{
	KBD_MOD_CTRL = 1,
	KBD_MOD_ALT = 2,
	KBD_MOD_SHIFT = 4
};

struct key_event
{
	uint8_t code;
	uint8_t mod;
};

enum kbd_state
{
	KBD_STATE_NORMAL,
	KBD_STATE_PREFIX
};

kbd_state state = KBD_STATE_NORMAL;
tty_device* cur_tty = nullptr;

constexpr char scancode_row_1[] =
{
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	'-',
	'=',
	'\b'
};

constexpr char scancode_row_2[] =
{
	'q',
	'w',
	'e',
	'r',
	't',
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	'\n'
};

constexpr char scancode_row_3[] =
{
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'',
	'`'
};

constexpr char scancode_row_4[] =
{
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/'
};

void interrupt_handler()
{
	auto scancode = io::inb(0x60);

	bool release_flag = scancode & 0x80;
	scancode = scancode & 0x7f;

	if(cur_tty && !release_flag)
	{
		if(scancode >= 0x02 && scancode <= 0x0e)
			tty_consume(cur_tty, scancode_row_1[scancode - 0x02]);
		else if(scancode >= 0x10 && scancode <= 0x1c)
			tty_consume(cur_tty, scancode_row_2[scancode - 0x10]);
		else if(scancode >= 0x1e && scancode <= 0x29)
			tty_consume(cur_tty, scancode_row_3[scancode - 0x1e]);
		else if(scancode >= 0x2b && scancode <= 0x35)
			tty_consume(cur_tty, scancode_row_4[scancode - 0x2b]);
		else if(scancode == 0x39)
			tty_consume(cur_tty, ' ');
	}
}

void init()
{
	log::info("ps2: initializing controller");

	io::outb(0xad, 0x64);
	io::outb(0xa7, 0x64);

	io::outb(0x20, 0x64);
	uint8_t config = io::inb(0x60);

	while(io::inb(0x64) & 1)
	{
		auto code = io::inb(0x60);
	}

	io::outb(0xae, 0x64);
	
	io::outb(0xff, 0x60);
	auto initcode = io::inb(0x60);
	initcode = io::inb(0x60);

	log::info("ps2: detected keyboard");

	auto irq = allocate_irq();
	ioapic::get(0).write_redirection_entry(0x1, irq);
	install_irq_handler(irq, interrupt_handler);
}

void set_tty(tty_device* tty)
{
	cur_tty = tty;
}

}
