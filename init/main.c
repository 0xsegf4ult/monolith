#include <syscall.h>
#include <stdio.h>

int main()
{
	int fd = open("/dev/tty0", 0);
	if(fd < 0)
		printf("Failed to open /dev/tty0\n");

	printf("INIT: booting...\n");

	spawn("/bin/sh");

	for(;;)
		asm volatile("pause");

	return -1;
}
