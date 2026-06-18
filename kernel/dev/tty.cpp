#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

#include <dev/tty.hpp>
#include <dev/ps2.hpp>
#include <dev/efifb.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>

#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <sys/mutex.hpp>

#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/err.hpp>

// only ECHO works
constexpr termios_t default_termios = {
	.c_iflag = ICRNL,
	.c_oflag = OPOST | ONLCR,
	.c_cflag = B38400,
	.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOCTL,
	.c_cc =
	{
		[VINTR] = 0x03,
		[VERASE] = 0x7F,
		[VSUSP] = 0x1A,
		[VEOF] = 0x04
	}
};

constexpr winsize_t default_winsize = {
	.ws_row = 25,
	.ws_col = 80,
	.ws_xpixel = 0,
	.ws_ypixel = 0
};

struct tty_device
{
	constexpr static size_t buffer_size = 0x1000;
	byte* read_buffer;
	uint32_t read_buffer_head;
	uint32_t read_buffer_tail;

	thread_t* waitqueue;
	mutex_t lock;

	termios_t termios;
	winsize_t winsize;
};

int tty_open(vfs::vnode_t* node, int flags)
{
	auto* thr = smp_current_cpu()->get_current_thread();

	auto minor = node->dev.minor();
	auto* tty = (tty_device*)(chardev_get(node->dev)->data);
	if(!tty)
		return -ENXIO;	

	if(!thr->tty)
	{
		thr->tty = tty;
	}

	return 0;
}

int tty_close(int fd)
{
	return 0;
}

void tty_consume(tty_device* tty, char c)
{
	mutex_lock(tty->lock);
	if((tty->read_buffer_tail + 1) % tty_device::buffer_size == tty->read_buffer_head)
	{
		mutex_unlock(tty->lock);
		return;
	}

	*reinterpret_cast<char*>(tty->read_buffer + tty->read_buffer_tail) = c;
       	tty->read_buffer_tail = (tty->read_buffer_tail + 1) % tty_device::buffer_size;

	/* send signal if ISIG 
	if(tty->termios.c_lflag & ISIG)
	*/

	if(tty->termios.c_iflag & ICRNL && c == '\r')
		c = '\n';

	if(tty->termios.c_lflag & ECHO)
		efifb_write(&c, 1u);

	while(tty->waitqueue)
	{
		auto* thr = tty->waitqueue;
		tty->waitqueue = tty->waitqueue->next;
		thr->next = nullptr;
		
		sched_unblock(thr);
	}
	mutex_unlock(tty->lock);
}

ssize_t tty_read(tty_device* tty, byte* buffer, size_t length)
{
	mutex_lock(tty->lock);
	while(tty->read_buffer_head == tty->read_buffer_tail)
	{
		auto* thr = smp_current_cpu()->get_current_thread();
	
		thr->next = tty->waitqueue;
		tty->waitqueue = thr;
	
		mutex_unlock(tty->lock);
		sched_block(thread_status::sleeping);
		mutex_lock(tty->lock);
	}

	size_t read_count = 0;
	if(tty->read_buffer_tail > tty->read_buffer_head)
		read_count = tty->read_buffer_tail - tty->read_buffer_head;
	else
		read_count = tty_device::buffer_size - tty->read_buffer_head + tty->read_buffer_tail;

	if(read_count > length)
		read_count = length;

	memcpy(buffer, tty->read_buffer + tty->read_buffer_head, read_count);
	tty->read_buffer_head = (tty->read_buffer_head + read_count) % tty_device::buffer_size;

	mutex_unlock(tty->lock);
	return read_count;
}

ssize_t tty_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	return tty_read(tty, buffer, length);
}

ssize_t tty_write(tty_device* tty, const byte* buffer, size_t length)
{
	for(size_t i = 0; i < length; i++)
	{
		if(!(char)buffer[i])
			break;

		if((tty->termios.c_oflag & ONLCR) && ((char)buffer[i] == '\n'))
		{
			char cr = '\r';
			efifb_write(&cr, 1);
		}
		efifb_write((const char*)buffer + i, 1);
	}

	return length;
}

ssize_t tty_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	return tty_write(tty, buffer, length);
}

int tty_ioctl(vfs::file_descriptor_t* file, uint64_t op, uint64_t arg)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	auto* thr = smp_current_cpu()->get_current_thread();
	if(!tty || tty != thr->tty)
		return -ENOTTY;

	switch(op)
	{
	case TCGETS:
	{
		termios_t tmp_t;
		mutex_lock(tty->lock);
		tmp_t = tty->termios;
		mutex_unlock(tty->lock);

		if(arg > 0x7fffffffffff)
			return -EFAULT;

		memcpy((byte*)arg, &tmp_t, sizeof(termios_t));
		return 0;
	}
	case TCSETS:
	{
		termios_t tmp_t;
		if(arg > 0x7fffffffffff)
			return -EFAULT;

		memcpy(&tmp_t, (byte*)arg, sizeof(termios_t));

		mutex_lock(tty->lock);
		tty->termios = tmp_t;
		mutex_unlock(tty->lock);

		return 0;
	}
	case TIOCGWINSZ:
	{
		winsize_t tmp_t;
		mutex_lock(tty->lock);
		tmp_t = tty->winsize;
		mutex_unlock(tty->lock);

		if(arg > 0x7fffffffffff)
			return -EFAULT;

		memcpy((byte*)arg, &tmp_t, sizeof(winsize_t));
		return 0;
	}
	}

	return -EINVAL;
}

static vfs::fs_file_ops tty_fops =
{
	.open = tty_open,
	.close = tty_close,
	.read = tty_read,
	.write = tty_write,
	.ioctl = tty_ioctl
};

void tty_init()
{
	auto* tty = chardev_alloc(dev_t{3, 0});
	tty->fops = &tty_fops;

	auto* device = (tty_device*)kmalloc(sizeof(tty_device));
	device->read_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size, vm_write));
	device->read_buffer_head = 0;
	device->read_buffer_tail = 0;
	device->waitqueue = nullptr;
	device->termios = default_termios;
	device->winsize = default_winsize;
	mutex_init(device->lock);
	tty->data = device;

	auto dev_node = vfs::mknod("/dev/tty0", S_IFCHR | S_IRUSR | S_IWUSR, dev_t{3, 0});
	ps2::set_tty(device);
}
