#include <dev/console.h>
#include <dev/efifb.h>
#include <dev/ps2.h>
#include <dev/tty.h>

#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>

#include <sys/device.h>

#include <types.h>

constexpr int num_virt_consoles = 1;
static struct tty_device* vt_tty[num_virt_consoles];
static int cur_vt = 0;

void console_write_internal(const char* data, size_t len)
{
	efifb_write(data, len);
}

ssize_t console_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	console_write_internal((const char*)buffer, length);
	return length;
}

static struct file_ops console_fops =
{
	.write = console_write
};

void console_init()
{
	for(int i = 0; i < num_virt_consoles; i++)
		vt_tty[i] = tty_create(i + 1, console_write_internal);

	ps2_set_tty(vt_tty[cur_vt]);

	dev_t id = make_dev(2, 0);
	struct char_device* console = chardev_new(id);
	console->fops = &console_fops;

	vfs_mknod("/dev/console", S_IFCHR | S_IRUSR | S_IWUSR, id);
}
