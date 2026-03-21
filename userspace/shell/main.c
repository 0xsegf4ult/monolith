#include <syscall.h>
#include <string.h>
#include <stdlib.h>

static int fd;
static char input_buf[256];
static char buffer[4096];
static size_t b_count;

void execute()
{
	if(b_count == 5 && (strncmp(buffer, "exit", 5) == 0)) 
		exit(0);		

	if(buffer[0] == '/')
	{
		int exec_fd = open(buffer, 0);
		if(exec_fd > 0)
		{
			write(fd, "\nexecute", 7);
			return;
		}
	}

	write(fd, "\nsh: ", 5);
	write(fd, buffer, b_count);
       	write(fd, ": command not found", 19);	
}

int main()
{
	fd = open("/dev/tty0", 0);
	ioctl(fd, 1, 0);

	const char* out_str = "\n[root@monolith]# ";
	write(fd, out_str, 19);

	b_count = 0;
	int running = 1;
	while(running)
	{
		ssize_t count = read(fd, input_buf, 256);

		for(ssize_t i = 0; i < count; i++)
		{
			char data = input_buf[i];
			if(data >= 32 && data <= 126 && b_count < 4095)
				buffer[b_count++] = data;

			if(data == '\b' && b_count)
				b_count--;

			if(data == '\n')
			{
				buffer[b_count++] = '\0';
				execute();
				b_count = 0;
				write(fd, out_str, 19);
				break;
			}

			write(fd, &data, 1);
		}
	}

	return 0;
}
