#include <syscall.hpp>

__attribute__((noinline)) void crash()
{
	*reinterpret_cast<char*>(0x123456) = 'a';
}

extern "C" void _start()
{
	int fd = open("/dev/tty0", 0);

	const char* out_str = "\n[root@monolith]# ";
	write(fd, out_str, 19);

	char input_buf[256];
	bool running = true;
	while(running)
	{
		auto count = read(fd, input_buf, 256);
	}

	_exit(0);
}
