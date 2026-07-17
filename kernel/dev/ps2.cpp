#include <dev/ps2.hpp>
#include <dev/tty.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <arch/x86_64/ioapic.hpp>
#include <arch/x86_64/io.hpp>
#include <arch/x86_64/interrupts.hpp>

#include <klog.hpp>
#include <types.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <sys/err.hpp>
#include <sys/scheduler.hpp>
#include <sys/task.hpp>
#include <sys/smp.hpp>

#include <kstd.hpp>

enum kbd_modifier_key : uint32_t
{
	KBD_MOD_CTRL = 1,
	KBD_MOD_ALT = 2,
	KBD_MOD_SHIFT = 4
};

struct key_event
{
	char key;
	bool state;
};

enum kbd_state
{
	KBD_STATE_NORMAL,
	KBD_STATE_PREFIX
};

static kbd_state state = KBD_STATE_NORMAL;
static tty_device* cur_tty = nullptr;
static task_t* owner = nullptr;
static key_event ringbuffer[256];
static int ring_head = 0;
static int ring_tail = 0;

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

constexpr char regular_scancodes[] =
{
	0,
	0x1b, // ESC
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
	0x7f, // DEL
	'\t',
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
	'\n',
	0,
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
	'`',
	0,
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
	'/',
	0,
	0,
	0,
	' '
};

constexpr char shifted_scancodes[] = 
{
	0,
	0x1b,
	'!',
	'@',
	'#',
	'$',
	'%',
	'^',
	'&',
	'*',
	'(',
	')',
	'_',
	'+',
	0x7f,
	0,
	'Q',
	'W',
	'E',
	'R',
	'T',
	'Y',
	'U',
	'I',
	'O',
	'P',
	'{',
	'}',
	'\n',
	0,
	'A',
	'S',
	'D',
	'F',
	'G',
	'H',
	'J',
	'K',
	'L',
	':',
	'"',
	'~',
	0,
	'|',
	'Z',
	'X',
	'C',
	'V',
	'B',
	'N',
	'M',
	'<',
	'>',
	'?',
	0,
	0,
	0,
	' '
};

static uint32_t keyboard0_mod = 0; 

static void interrupt_handler()
{
	auto scancode = io::inb(0x60);

	bool release_flag = scancode & 0x80;
	scancode = scancode & 0x7f;

	if(scancode == 0x2a || scancode == 0x36)
	{
		if(release_flag)
			keyboard0_mod &= ~(KBD_MOD_SHIFT);
		else
			keyboard0_mod |= KBD_MOD_SHIFT;
	}

	if(scancode == 0x1d)
	{
		if(release_flag)
			keyboard0_mod &= ~(KBD_MOD_CTRL);
		else
			keyboard0_mod |= KBD_MOD_CTRL;
	}

	bool shift = keyboard0_mod & KBD_MOD_SHIFT;
	bool ctrl = keyboard0_mod & KBD_MOD_CTRL;
	bool alt = keyboard0_mod & KBD_MOD_ALT;

	if(!owner && cur_tty && !release_flag)
	{
		const char* reg_tbl = shift ? &shifted_scancodes[0] : &regular_scancodes[0];
		if(scancode == 0x2e && ctrl)
		{
			tty_consume(cur_tty, 0x03);
			return;
		}	

		if(scancode <= 0x39 && reg_tbl[scancode])
			tty_consume(cur_tty, reg_tbl[scancode]);


		/*
			tty_consume(cur_tty, scancode_row_1[scancode - 0x02]);
		else if(scancode >= 0x10 && scancode <= 0x1c)
			tty_consume(cur_tty, scancode_row_2[scancode - 0x10]);
		else if(scancode >= 0x1e && scancode <= 0x29)
			tty_consume(cur_tty, scancode_row_3[scancode - 0x1e]);
		else if(scancode >= 0x2b && scancode <= 0x35)
			tty_consume(cur_tty, scancode_row_4[scancode - 0x2b]);
		else if(scancode == 0x39)
			tty_consume(cur_tty, ' ');*/
	}
	
	if(owner)
	{
		const char* reg_tbl = shift ? &shifted_scancodes[0] : &regular_scancodes[0];
		if(scancode <= 0x39)
		{
			if((ring_tail + 1) % 256 == ring_head)
				return;
		
			if(reg_tbl[scancode])
				ringbuffer[ring_tail] = {reg_tbl[scancode], !release_flag};
			else if(scancode == 0x1d)
				ringbuffer[ring_tail] = {0x1d, !release_flag};

			ring_tail = (ring_tail + 1) % 256;
		}
	}
}

int kbd_open(vfs::vnode_t* node, int flags)
{
	if(owner)
		return -EBUSY;

	owner = smp_current_task();
	return 0;
}

int kbd_close(int fd)
{
	owner = nullptr;
	return 0;
}

// hacky 'nonblocking' read
ssize_t kbd_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	if(smp_current_task() != owner)
		return -EBUSY;

	if(ring_head == ring_tail)
		return 0;

	memcpy(buffer, &ringbuffer[ring_head], sizeof(key_event));
	ring_head = (ring_head + 1) % 256;

	return 2;
}

static vfs::fs_file_ops kbd_fops =
{
	.open = kbd_open,
	.close = kbd_close,
	.read = kbd_read
};

void ps2_init()
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

	auto* dev = chardev_alloc(dev_t{10, 0});
	dev->fops = &kbd_fops;

	auto dev_node = vfs::mknod("/dev/keyboard", S_IFCHR | S_IRUSR | S_IWUSR, dev_t{10, 0});

	auto irq = allocate_irq();
	ioapic_write_redirection_entry(0x1, irq);
	install_irq_handler(irq, interrupt_handler);
}

void ps2_set_tty(tty_device* tty)
{
	cur_tty = tty;
}
