#include <arch/x86_64/cpu.hpp>

#include <dev/tty.hpp>
#include <dev/ps2.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>

#include <sys/process.hpp>
#include <sys/scheduler.hpp>

#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/err.hpp>

struct tty_device
{
	constexpr static size_t buffer_size = 0x1000;
	byte* read_buffer;
	uint32_t read_buffer_head;
	uint32_t read_buffer_tail;

	process_t* waitqueue;

	bool echo;
};

int tty_open(vfs::vnode_t* node, int flags)
{
	return node->dev.minor();
}

int tty_close(int fd)
{
	return 0;
}

void tty_consume(tty_device* tty, char c)
{
	if((tty->read_buffer_tail + 1) % tty_device::buffer_size == tty->read_buffer_head)
		return;

	*reinterpret_cast<char*>(tty->read_buffer + tty->read_buffer_tail) = c;
       	tty->read_buffer_tail = (tty->read_buffer_tail + 1) % tty_device::buffer_size;

	if(tty->echo)
		klog_internal(&c, 1);	

	while(tty->waitqueue)
	{
		auto* proc = tty->waitqueue;
		tty->waitqueue = tty->waitqueue->next;
		proc->next = nullptr;
		
		sched_unblock(proc);
	}
}

ssize_t tty_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	while(tty->read_buffer_head == tty->read_buffer_tail)
	{
		auto* proc = CPU::get_current()->get_current_process();
		proc->next = tty->waitqueue;
		tty->waitqueue = proc;
	
		sched_block(process_status::sleeping);
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

	return read_count;
}

ssize_t tty_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);

	klog_internal((const char*)buffer, length);

	return length;
}

int tty_ioctl(vfs::file_descriptor_t* file, uint64_t op, uint64_t arg)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);

	if(op == 1)
	{
		tty->echo = (arg > 0);
		return 0;
	}

	return -ENOTTY;
}

static vfs::fs_ops tty_fops =
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
	tty->ops = &tty_fops;

	auto* device = (tty_device*)kmalloc(sizeof(tty_device));
	device->read_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size, vm_write));
	device->read_buffer_head = 0;
	device->read_buffer_tail = 0;
	device->waitqueue = nullptr;
	device->echo = true;
	tty->data = device;

	auto dev_node = vfs::mknod("/dev/tty0", 'c', dev_t{3, 0});
	ps2::set_tty(device);
}
