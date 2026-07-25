#include <dev/ps2.h>
#include <dev/tty.h>

#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>

#include <sched/scheduler.h>
#include <sched/task.h>
#include <sys/device.h>
#include <sys/irq.h>
#include <sys/smp.h>

#include <libk/string.h>

#include <io.h>

#include <ernno.h>
#include <klog.h>
#include <types.h>

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

typedef enum
{
	KBD_STATE_NORMAL,
	KBD_STATE_PREFIX
} kbd_state;

static kbd_state state = KBD_STATE_NORMAL;
static struct tty_device* cur_tty = nullptr;
static struct task* owner = nullptr;
static struct key_event ringbuffer[256];
static int ring_head = 0;
static int ring_tail = 0;

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

static void interrupt_handler(void* payload)
{
	auto scancode = inb(0x60);

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
	}
	
	if(owner)
	{
		const char* reg_tbl = shift ? &shifted_scancodes[0] : &regular_scancodes[0];
		if(scancode <= 0x39)
		{
			if((ring_tail + 1) % 256 == ring_head)
				return;
		
			if(reg_tbl[scancode])
			{
				ringbuffer[ring_tail].key = reg_tbl[scancode]; 
				ringbuffer[ring_tail].state = !release_flag;
			}
			else if(scancode == 0x1d)
			{
				ringbuffer[ring_tail].key = 0x1d;
				ringbuffer[ring_tail].state = !release_flag;
			}

			ring_tail = (ring_tail + 1) % 256;
		}
	}
}

int kbd_open(struct vnode* node, int flags)
{
	if(owner)
		return -EBUSY;

	owner = smp_current_task();
	return 0;
}

int kbd_close(struct file_descriptor* file)
{
	owner = nullptr;
	return 0;
}

// hacky 'nonblocking' read
ssize_t kbd_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	if(smp_current_task() != owner)
		return -EBUSY;

	if(ring_head == ring_tail)
		return 0;

	memcpy(buffer, &ringbuffer[ring_head], sizeof(struct key_event));
	ring_head = (ring_head + 1) % 256;

	return 2;
}

static struct file_ops kbd_fops =
{
	.open = kbd_open,
	.close = kbd_close,
	.read = kbd_read
};

constexpr hwirq_t ISA_IRQ_PS2 = 1;

void ps2_init()
{
	klog("ps2: initializing controller\n");

	outb(0xad, 0x64);
	outb(0xa7, 0x64);

	outb(0x20, 0x64);
	uint8_t config = inb(0x60);

	while(inb(0x64) & 1)
	{
		uint8_t code = inb(0x60);
	}

	outb(0xae, 0x64);
	
	outb(0xff, 0x60);
	uint8_t initcode = inb(0x60);
	initcode = inb(0x60);

	klog("ps2: detected keyboard\n");

	dev_t id = make_dev(10, 0);
	struct char_device* dev = chardev_new(id);
	dev->fops = &kbd_fops;

	vfs_mknod("/dev/keyboard", S_IFCHR | S_IRUSR | S_IWUSR, id);
	irq_register(ISA_IRQ_PS2, interrupt_handler, nullptr);
}

void ps2_set_tty(struct tty_device* tty)
{
	cur_tty = tty;
}
