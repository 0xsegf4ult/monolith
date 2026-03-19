#include <syscall.hpp>

extern "C" int _start()
{
	debugmsg("Message from userspace!");	

	int fd = open("/dev/tty0", 0);
	if(fd < 0)
		debugmsg("Failed to open /dev/tty0");

	const char* out_str = "INIT: booting...\n";
	write(fd, out_str, 17);

	spawn("/bin/sh");

	for(;;)
		asm volatile("pause");
}
