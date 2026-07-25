#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

int main(int argc, const char** argv)
{
	if(argc < 4)
	{
		printf("mount: missing operand\n");
		return 0;
	}

	int status = mount(argv[1], argv[2], argv[3], 0, nullptr);
	if(status < 0)
	{
		printf("mount: cannot mount %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
