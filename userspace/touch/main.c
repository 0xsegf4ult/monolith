#include <stdio.h>
#include <string.h>
#include <syscall.h>

int main(int argc, const char** argv)
{
	if(argc < 2)
	{
		printf("touch: missing file operand");
		return 0;
	}


	int fd = open(argv[1], O_CREAT);
	if(fd < 0)
	{
		printf("touch: cannot touch %s: %s", argv[1], strerrordesc_np(-fd));
		return fd;
	}

	close(fd);

	return 0;
}
