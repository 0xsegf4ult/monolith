#include <dev/console.hpp>
#include <dev/character.hpp>
#include <dev/device.hpp>
#include <dev/efifb.hpp>
#include <dev/ps2.hpp> 
#include <dev/tty.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <types.hpp>

constexpr int num_virt_consoles = 1;
static tty_device* vt_tty[num_virt_consoles];
static int cur_vt = 0;

void console_write_internal(const char* data, size_t len)
{
	efifb_write(data, len);
}

ssize_t console_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	console_write_internal((const char*)buffer, length);
	return length;
}

static vfs::fs_file_ops console_fops =
{
	.write = console_write
};

void console_init()
{
	for(int i = 0; i < num_virt_consoles; i++)
		vt_tty[i] = tty_create(i + 1, console_write_internal);

	ps2::set_tty(vt_tty[cur_vt]);

	auto* console = chardev_alloc(dev_t{2, 0});
	console->fops = &console_fops;

	auto dev_node = vfs::mknod("/dev/console", S_IFCHR | S_IRUSR | S_IWUSR, dev_t{2, 0});
}
