#include <stdio.h>
#include <string.h>
#include <syscall.h>

int main(int argc, const char** argv)
{
	if(argc < 2)
	{
		printf("mkdir: missing operand");
		return 0;
	}

	int status = mkdir(argv[1]);
	if(status < 0)
	{
		printf("touch: cannot create directory %s: %s", argv[1], strerrordesc_np(-status));
		return status;
	}

	return 0;
}
