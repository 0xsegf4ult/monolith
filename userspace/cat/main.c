#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char buffer[0x1000];

int main(int argc, const char** argv)
{
	int fd;

	if(argc < 2)
	{
		fd = 0;
	}
	else
	{
		fd = open(argv[1], 0);
		if(fd < 0)
		{
			printf("cat: %s: %s", argv[1], strerrordesc_np(-fd));
			return -1;
		}

		struct stat stat_info;
		int st_r = fstat(fd, &stat_info);
		if(st_r < 0)
		{
			printf("cat: %s: %s", argv[1], strerrordesc_np(-st_r));
			return -1;
		}

		if(S_ISDIR(stat_info.st_mode))
		{
			printf("cat: %s: Is a directory", argv[1]);
			return -1;
		}
	}

	ssize_t bytes_read = read(fd, buffer, 0x1000);
	if(bytes_read < 0)
	{
		printf("cat: %s: %s", argc < 2 ? "stdin" : argv[1], strerrordesc_np(-bytes_read));
		goto cleanup;
	}
	write(0, buffer, 0x1000);
cleanup:
	close(fd);
	return 0;
}
