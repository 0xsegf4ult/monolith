#include <dev/tty.h>
#include <dev/console.h>

#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>

#include <mm/slab.h>
#include <mm/vmm.h>

#include <sched/scheduler.h>
#include <sched/signal.h>
#include <sched/task.h>

#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <libk/string.h>
#include <libk/vsprintf.h>

#include <errno.h>
#include <panic.h>

constexpr struct termios default_termios = {
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

constexpr struct winsize default_winsize = {
	.ws_row = 25,
	.ws_col = 80,
	.ws_xpixel = 0,
	.ws_ypixel = 0
};

int tty_open(struct vnode* node, int flags)
{
	struct task* task = smp_current_task();

	struct tty_device* tty = (struct tty_device*)(chardev_lookup(node->dev)->data);
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

void tty_add_input(struct tty_device* tty, const char* c, size_t len)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&tty->write_buffer_lock, &flags);

	spinlock_release_irqsave(&tty->write_buffer_lock, flags);
}

void tty_consume(struct tty_device* tty, char c)
{
	mutex_lock(&tty->lock);
	if((tty->read_buffer_tail + 1) % tty_buffer_size == tty->read_buffer_head)
	{
		mutex_unlock(&tty->lock);
		return;
	}

	*(char*)(tty->read_buffer + tty->read_buffer_tail) = c;
       	tty->read_buffer_tail = (tty->read_buffer_tail + 1) % tty_buffer_size;

	if(tty->termios.c_lflag & ISIG)
	{
		if(c == tty->termios.c_cc[VINTR])
		{
			mutex_unlock(&tty->lock);
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

	wait_queue_wake(&tty->waitqueue);
	mutex_unlock(&tty->lock);
}

ssize_t tty_read(struct tty_device* tty, byte* buffer, size_t length)
{
	mutex_lock(&tty->lock);
	if(tty->read_buffer_head == tty->read_buffer_tail)
	{
		struct task* task = smp_current_task();
		int wret = 0;
		while(1)
		{
			wait_queue_register(&tty->waitqueue, &task->wait);
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

			mutex_unlock(&tty->lock);
			sched_yield();
			mutex_lock(&tty->lock);
		}
		wait_queue_unregister(&task->wait);
		task_status exp_state = TASK_INTR_SLEEPING;
		atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING);
		if(wret < 0)
		{
			mutex_unlock(&tty->lock);
			return wret;
		}
	}

	size_t read_count = 0;
	if(tty->read_buffer_tail > tty->read_buffer_head)
		read_count = tty->read_buffer_tail - tty->read_buffer_head;
	else
		read_count = tty_buffer_size - tty->read_buffer_head + tty->read_buffer_tail;

	if(read_count > length)
		read_count = length;

	memcpy(buffer, tty->read_buffer + tty->read_buffer_head, read_count);
	tty->read_buffer_head = (tty->read_buffer_head + read_count) % tty_buffer_size;

	mutex_unlock(&tty->lock);
	return read_count;
}

ssize_t tty_read_file(struct file_descriptor* file, byte* buffer, size_t length)
{
	struct tty_device* tty = (struct tty_device*)(chardev_lookup(file->inode->dev)->data);
	return tty_read(tty, buffer, length);
}

ssize_t tty_write(struct tty_device* tty, const byte* buffer, size_t length)
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

ssize_t tty_write_file(struct file_descriptor* file, const byte* buffer, size_t length)
{
	struct tty_device* tty = (struct tty_device*)(chardev_lookup(file->inode->dev)->data);
	return tty_write(tty, buffer, length);
}

int tty_ioctl(struct file_descriptor* file, uint64_t op, uint64_t arg)
{
	struct tty_device* tty = (struct tty_device*)(chardev_lookup(file->inode->dev)->data);
	struct task* task = smp_current_task();
	if(!tty || tty != task->tty)
		return -ENOTTY;

	switch(op)
	{
	case TIOCSPGRP:
	{
		if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;
		
		if(task->sid != tty->session_id)
			return -ENOTTY;

		pid_t pgrp;
		memcpy(&pgrp, (byte*)arg, sizeof(pid_t));
		struct task* leader = get_pgrp_leader(pgrp);
		if(!leader || leader->sid != task->sid)
			return -EPERM;

		tty->fg_pgrp = pgrp;

		return 0;
	}		
	case TCGETS:
	{
		if(!arg || arg > 0x7fffffffffff)
			return -EFAULT;
		
		mutex_lock(&tty->lock);
		memcpy((void*)arg, &tty->termios, sizeof(struct termios));
		mutex_unlock(&tty->lock);


		return 0;
	}
	case TCSETS:
	{
		if(!arg || arg > 0x7fffffffffff)
			return -EFAULT;

		mutex_lock(&tty->lock);
		memcpy(&tty->termios, (void*)arg, sizeof(struct termios));
		mutex_unlock(&tty->lock);

		return 0;
	}
	case TIOCGWINSZ:
	{
		if(arg > 0x7fffffffffff)
			return -EFAULT;
		
		mutex_lock(&tty->lock);
		memcpy((byte*)arg, &tty->winsize, sizeof(struct winsize));
		mutex_unlock(&tty->lock);

		return 0;
	}
	}

	return -EINVAL;
}

static struct file_ops tty_fops =
{
	.open = tty_open,
	.close = tty_close,
	.read = tty_read_file,
	.write = tty_write_file,
	.ioctl = tty_ioctl
};

struct tty_device* tty_create(uint16_t index, tty_output_t output_fn)
{
	dev_t id = make_dev(3, index);
	struct char_device* tty = chardev_new(id);
	tty->fops = &tty_fops;

	struct tty_device* device = kmalloc(sizeof(struct tty_device));

	device->read_buffer = vmalloc(tty_buffer_size);
	device->read_buffer_head = 0;
	device->read_buffer_tail = 0;

	device->write_buffer = vmalloc(tty_buffer_size);
	device->write_buffer_head = 0;
	device->write_buffer_tail = 0;

	spinlock_init(&device->write_buffer_lock);

	wait_queue_init(&device->waitqueue);
	device->output = output_fn;
	device->termios = default_termios;
	device->winsize = default_winsize;
	device->session_id = 0;
	device->fg_pgrp = 0;
	mutex_init(&device->lock);
	tty->data = device;

	char devname[32];
	sprintf(devname, "/dev/tty%u", index);
	vfs_mknod(devname, S_IFCHR | S_IRUSR | S_IWUSR, id);

	return device;
}
