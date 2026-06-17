#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static char input_buf[256];
static char buffer[4096];
static size_t b_count;
static int bindir;

void execute()
{
/*	size_t sp_offset = 0;
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
	if(sp_offset + 1 <= b_count)
		argv[1] = buffer + sp_offset + 1;
	else
		argv[1] = nullptr;

	argv[2] = nullptr;
*/
	const char* argv[8];
	int argc = 0;
	char* token = strtok(buffer, " \n");
	while(token)
	{
		argv[argc] = token;
		token = strtok(NULL, " \n");
		argc++;
		if(argc == 8)
			break;
	}

	if(argc < 8)
	{
		for(int i = argc; i < 8; i++)
			argv[i] = NULL;
	}

	if(buffer[0] == '/' || buffer[0] == '.')
	{
		stat_t ex_stat;
		int st_r = stat(buffer, &ex_stat);
		if(st_r < 0)
		{
			printf("\nsh: %s: %s\n", buffer, strerrordesc_np(-st_r));
			return;
		}

		if(S_ISDIR(ex_stat.mode))
		{
			printf("\nsh: %s: Is a directory\n", buffer);
			return;
		}

		if(S_ISREG(ex_stat.mode))
		{
			printf("\n");
			int s_res = spawn(argv);
			if(s_res < 0)
			{
				printf("sh: %s: %s\n", argv[0], strerrordesc_np(-s_res));
			}
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
	else
	{
		auto ex_fd = openat(bindir, buffer, 0);
		if(ex_fd)
		{
			stat_t ex_stat;
			int st_r = fstat(ex_fd, &ex_stat);
			close(ex_fd);
			if(st_r >= 0 && S_ISREG(ex_stat.mode))
			{
				printf("\n");
				int s_res = spawnat(bindir, argv);
				if(s_res < 0)
				{
					printf("sh: %s: %s\n", argv[0], strerrordesc_np(-s_res));
				}
				wait();
				return;
			}
		}
	}

	printf("\nsh: %s: command not found\n", buffer);
}

int main()
{
	ioctl(0, 1, 0);

	bindir = open("/bin", 0);

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
			{
				buffer[b_count] = '\0';
				b_count--;
			}

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

	close(bindir);
	return 0;
}
