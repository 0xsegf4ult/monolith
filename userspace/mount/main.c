#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

int main(int argc, const char** argv)
{
	if(argc < 4)
	{
		printf("mkdir: missing operand");
		return 0;
	}

	int status = mount(argv[1], argv[2], argv[3]);
	if(status < 0)
	{
		printf("mount: cannot mount %s: %s", argv[1], strerrordesc_np(-status));
		return status;
	}

	return 0;
}
