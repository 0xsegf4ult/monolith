#include <dev/tty.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <lib/klog.hpp>

struct tty_device
{
	constexpr static size_t buffer_size = 0x1000;
	byte* read_buffer;
	byte* write_buffer;
	uint32_t read_buffer_head;
	uint32_t write_buffer_head;
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

size_t tty_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* tty = (tty_device*)(chardev_get(file->inode->dev)->data);
	while(!tty->read_buffer_head)
	{
		sched_block();
	}

	return 0;
}

size_t tty_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	return 0;
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
	device->write_buffer = reinterpret_cast<byte*>(vmalloc(tty_device::buffer_size, vm_write));
	device->read_buffer_head = 0;
	device->write_buffer_head = 0;
	tty->data = device;

	auto dev_node = vfs::mknod("/dev/tty0", 'c', dev_t{3, 0});
}
