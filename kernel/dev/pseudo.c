#include <dev/pseudo.h>
#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <sys/device.h>
#include <libk/string.h>
#include <errno.h>

ssize_t null_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	return 0;
}

ssize_t null_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	return 0;
}

ssize_t zero_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	memset(buffer, 0, length);
	return length;
}

ssize_t full_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	return -ENOSPC;
}

static struct file_ops null_fops =
{
	.read = null_read,
	.write = null_write
};

static struct file_ops full_fops =
{
	.read = zero_read,
	.write = full_write
};

static struct file_ops zero_fops =
{
	.read = zero_read,
	.write = null_write
};

void pseudo_init()
{
	dev_t ndev = make_dev(1, 0);
	struct char_device* null = chardev_new(ndev);
	null->fops = &null_fops;
	vfs_mknod("/dev/null", S_IFCHR | 0666, ndev);

	dev_t fdev = make_dev(1, 1);
	struct char_device* full = chardev_new(fdev);
	full->fops = &full_fops;
	vfs_mknod("/dev/full", S_IFCHR | 0666, fdev);

	dev_t zdev = make_dev(1, 2);
	struct char_device* zero = chardev_new(zdev);
	zero->fops = &zero_fops;
	vfs_mknod("/dev/zero", S_IFCHR | 0666, zdev);
}
