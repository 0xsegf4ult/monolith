#include <syscall.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char input_buf[256];
static char buffer[4096];
static size_t b_count;

void execute()
{
	size_t sp_offset = 0;
	const char* cur = buffer;
	while(cur && sp_offset < b_count)
	{
		if(*cur == ' ')
			break;

		sp_offset++;
		cur++;
	}

	buffer[sp_offset] = '\0';

	const char* argv[3];
	argv[0] = buffer;
	argv[1] = buffer + sp_offset + 1;
	if(argv[1][0] == '\0')
		argv[1] = nullptr;
	else
		printf("\nargument: %s\n", argv[1]);
	argv[2] = nullptr;

	if(buffer[0] == '/')
	{
		stat_t ex_stat;
		int st_r = stat(buffer, &ex_stat);
		if(st_r < 0)
		{
			printf("\nsh: %s: %s\n", buffer, strerrordesc_np(-st_r));
			return;
		}

		if(ex_stat.mode == S_IFDIR)
		{
			printf("\nsh: %s: Is a directory\n", buffer);
			return;
		}

		if(ex_stat.mode == S_IFREG)
		{
			printf("\n");
			spawn(buffer, argv);
			wait();
			return;
		}
	}
	else if(strncmp(buffer, "cd", 2) == 0)
	{
		if(argv[1][0])
		{
			int r = chdir(argv[1]);
			if(r < 0)
				printf("\nsh: cd: %s: %s", argv[1], strerrordesc_np(-r));
		}

		printf("\n");
		return;
	}
	else if(b_count == 5 && (strncmp(buffer, "exit", 5) == 0)) 
		exit(0);		


	printf("\nsh: %s: command not found\n", buffer);
}

int main()
{
	ioctl(0, 1, 0);

	printf("[root@monolith]# ");

	b_count = 0;
	int running = 1;
	while(running)
	{
		ssize_t count = read(0, input_buf, 256);

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
				printf("[root@monolith]# ");
				break;
			}

			write(0, &data, 1);
		}
	}

	return 0;
}
