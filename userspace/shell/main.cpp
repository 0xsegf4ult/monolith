#include <syscall.hpp>

extern "C" void _start()
{
	int fd = open("/dev/tty0", 0);

	const char* out_str = "[root@monolith]# \n";
	write(fd, out_str, 19);

	_exit(0);
}
