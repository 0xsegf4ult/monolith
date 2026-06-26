#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char buffer[0x1000];

int main(int argc, const char** argv)
{
	int fd;

	if(argc >= 2)
	{
		fd = open(argv[1], O_CREAT);
		if(fd < 0)
		{
			printf("tee: %s: %s", argv[1], strerrordesc_np(-fd));
			return -1;
		}
	}

	do
	{
		ssize_t bytes_read = read(0, buffer, 0x1000);
		if(bytes_read < 0)
		{
			printf("tee: stdin: %s", strerrordesc_np(-bytes_read));
			goto cleanup;
		}
		
		if(bytes_read == 0)
			break;

		if(buffer[0] == 0x03)
			break;

		write(0, buffer, bytes_read);
		if(fd)
		{
			ssize_t bw = write(fd, buffer, bytes_read);
			if(bw < 0)
			{
				printf("tee: %s: %s", argv[1], strerrordesc_np(-bw));
				goto cleanup;
			}
		}
	} while(1);
cleanup:
	if(fd) close(fd);
	return 0;
}
