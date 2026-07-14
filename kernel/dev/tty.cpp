#include <dev/tty.hpp>
#include <dev/console.hpp>
#include <dev/character.hpp>
#include <dev/device.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/err.hpp>

#include <sys/mutex.hpp>
#include <sys/scheduler.hpp>
#include <sys/signal.hpp>
#include <sys/smp.hpp>
#include <sys/task.hpp>

#include <klog.hpp>
#include <kstd.hpp>
#include <panic.hpp>

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

int tty_open(vfs::vnode_t* node, int flags)
{
	auto* task = smp_current_task();

	auto minor = node->dev.minor();
	auto* tty = (tty_device*)(chardev_get(node->dev)->data);
	if(!tty)
		return -ENXIO;	

	if(!task->tty && is_session_leader(task) && tty->session_id == 0)
	{
		task->tty = tty;
		tty->session_id = task->sid;
		tty->fg_pgrp = task->pgid;
	}

	return 0;
}

int tty_close(int fd)
{
	return 0;
}

void tty_add_input(tty_device* tty, const char* c, size_t len)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(tty->write_buffer_lock, rflags);

	spinlock_release_irqsave(tty->write_buffer_lock, rflags);
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

	if(tty->termios.c_lflag & ISIG)
	{
		if(c == tty->termios.c_cc[VINTR])
		{
			mutex_unlock(tty->lock);
			log::debug("sending SIGINT to pgrp {}", tty->fg_pgrp);
			pgrp_send_signal(tty->fg_pgrp, SIGINT);
			return;
		}
	}

	if(tty->termios.c_iflag & ICRNL && c == '\r')
		c = '\n';

	if(tty->termios.c_lflag & ECHO)
	{
		tty->output(&c, 1u);
	}

	wait_queue_wake(tty->waitqueue);
	mutex_unlock(tty->lock);
}

ssize_t tty_read(tty_device* tty, byte* buffer, size_t length)
{
	mutex_lock(tty->lock);
	if(tty->read_buffer_head == tty->read_buffer_tail)
	{
		auto* task = smp_current_task();
		int wret = 0;
		while(1)
		{
			wait_queue_register(tty->waitqueue, task->wait);
			task_status exp_state = TASK_RUNNING;
			if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_INTR_SLEEPING))	
			{
				if(exp_state != TASK_INTR_SLEEPING)
					panic("wq_register cmpxchg failed");
			}

			if(tty->read_buffer_head != tty->read_buffer_tail)
				break;

			if(signal_pending(task))
			{
				wret = -EINTR;
				break;
			}

			mutex_unlock(tty->lock);
			sched_yield();
			mutex_lock(tty->lock);
		}
		wait_queue_unregister(task->wait);
		task_status exp_state = TASK_INTR_SLEEPING;
		atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING);
		if(wret < 0)
		{
			mutex_unlock(tty->lock);
			return wret;
		}
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
			tty->output(&cr, 1);
		}
		tty->output((const char*)buffer + i, 1);
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
	auto* task = smp_current_task();
	if(!tty || tty != task->tty)
		return -ENOTTY;

	switch(op)
	{
	case TIOCSPGRP:
	{
		pid_t pgrp;
		if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;
		
		if(task->sid != tty->session_id)
			return -ENOTTY;

		memcpy(&pgrp, (byte*)arg, sizeof(pid_t));
		auto* leader = get_pgrp_leader(pgrp);
		if(!leader || leader->sid != task->sid)
			return -EPERM;

		tty->fg_pgrp = pgrp;

		return 0;
	}		
	case TCGETS:
	{
		termios_t tmp_t;
		mutex_lock(tty->lock);
		tmp_t = tty->termios;
		mutex_unlock(tty->lock);

		if(!arg || arg > 0x7fffffffffff)
			return -EFAULT;

		memcpy((byte*)arg, &tmp_t, sizeof(termios_t));
		return 0;
	}
	case TCSETS:
	{
		termios_t tmp_t;
		if(!arg || arg > 0x7fffffffffff)
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

tty_device* tty_create(uint16_t index, tty_output_t output_fn)
{
	auto* tty = chardev_alloc(dev_t{3, index});
	tty->fops = &tty_fops;

	auto* device = (tty_device*)kmalloc(sizeof(tty_device));

	device->read_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size));
	device->read_buffer_head = 0;
	device->read_buffer_tail = 0;

	device->write_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size));
	device->write_buffer_head = 0;
	device->write_buffer_tail = 0;

	spinlock_init(device->write_buffer_lock);

	wait_queue_init(device->waitqueue);
	device->output = output_fn;
	device->termios = default_termios;
	device->winsize = default_winsize;
	device->session_id = 0;
	device->fg_pgrp = 0;
	mutex_init(device->lock);
	tty->data = device;

	char devname[32];
	format_to(string_span{&devname[0], 32}, "/dev/tty{}", index);
	auto dev_node = vfs::mknod(devname, S_IFCHR | S_IRUSR | S_IWUSR, dev_t{3, index});

	return device;
}
