#include <arch/x86_64/cpu.hpp>

#include <dev/tty.hpp>
#include <dev/ps2.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <lib/klog.hpp>

#include <sys/process.hpp>
#include <sys/scheduler.hpp>

#include <mm/slab.hpp>
#include <mm/vmm.hpp>

struct tty_device
{
	constexpr static size_t buffer_size = 0x1000;
	byte* read_buffer;
	uint32_t read_buffer_head;

	process_t* waitqueue = nullptr;
};

int tty_open(vfs::vnode_t* node, int flags)
{
	log::debug("open() tty0");
	return node->dev.minor();
}

int tty_close(int fd)
{
	return 0;
}

void tty_consume(tty_device* tty, char c)
{
	if(tty->read_buffer_head >= tty_device::buffer_size)
		return;
	
	log::debug("tty0 consume {}", c);
	*reinterpret_cast<char*>(tty->read_buffer + tty->read_buffer_head) = c;
       	tty->read_buffer_head++;

	while(tty->waitqueue)
	{
		auto* proc = tty->waitqueue;
		tty->waitqueue = tty->waitqueue->next;
		proc->next = nullptr;
		sched_unblock(proc);
	}
}

size_t tty_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	while(!tty->read_buffer_head)
	{
		auto* proc = CPU::get_current()->get_current_process();
		proc->next = tty->waitqueue;
		tty->waitqueue = proc;
		
		sched_block(process_status::sleeping);
	}

	auto count = tty->read_buffer_head;

	return 0;
}

size_t tty_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);

	klog_internal((const char*)buffer);

	return length;
}

static vfs::fs_ops tty_fops =
{
	.open = tty_open,
	.close = tty_close,
	.read = tty_read,
	.write = tty_write
};

void tty_init()
{
	auto* tty = chardev_alloc(dev_t{3, 0});
	tty->ops = &tty_fops;

	auto* device = (tty_device*)kmalloc(sizeof(tty_device));
	device->read_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size, vm_write));
	device->read_buffer_head = 0;
	tty->data = device;

	auto dev_node = vfs::mknod("/dev/tty0", 'c', dev_t{3, 0});
	ps2::set_tty(device);
}
