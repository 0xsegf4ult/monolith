#include <syscall.h>
#include <stdio.h>

int main()
{
	int fd = open("/dev/tty0", 0);
	if(fd < 0)
		printf("Failed to open /dev/tty0\n");

	printf("  monolith x86_64 0.01.2 is starting up\n\n");
	printf("logged in as root on /dev/tty0\n");

	const char* argv[2] = {"/bin/sh", nullptr};
	for(;;)
	{
		spawn("/bin/sh", argv);
		wait();
		printf("\ninit: /bin/sh exited\n");
	}

	return 0;
}
