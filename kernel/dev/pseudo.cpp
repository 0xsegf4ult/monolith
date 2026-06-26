#include <dev/pseudo.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <kstd.hpp>
#include <klog.hpp>

#include <sys/err.hpp>

ssize_t null_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	return 0;
}

ssize_t null_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	return 0;
}

ssize_t zero_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	memset(buffer, 0, length);
	return length;
}

ssize_t full_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	return -ENOSPC;
}

static vfs::fs_file_ops null_fops =
{
	.read = null_read,
	.write = null_write
};

static vfs::fs_file_ops full_fops =
{
	.read = zero_read,
	.write = full_write
};

static vfs::fs_file_ops zero_fops =
{
	.read = zero_read,
	.write = null_write
};

void pseudo_init()
{
	auto* null = chardev_alloc(dev_t{1, 0});
	null->fops = &null_fops;
	auto null_node = vfs::mknod("/dev/null", S_IFCHR | 0666, dev_t{1, 0});

	auto* full = chardev_alloc(dev_t{1, 1});
	full->fops = &full_fops;
	auto full_node = vfs::mknod("/dev/full", S_IFCHR | 0666, dev_t{1, 1});

	auto* zero = chardev_alloc(dev_t{1, 2});
	zero->fops = &zero_fops;
	auto zero_node = vfs::mknod("/dev/zero", S_IFCHR | 0666, dev_t{1, 2});
}
