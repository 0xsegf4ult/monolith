#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/spawn.h>
#include <termios.h>
#include <unistd.h>

static char input_buf[256];
static char buffer[4096];
static size_t b_count;

void execute()
{
	const char* argv[9];
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

	argv[8] = NULL;

	if(strncmp(buffer, "cd", 2) == 0)
	{
		if(argv[1] && argv[1][0])
		{
			int r = chdir(argv[1]);
			if(r < 0)
				printf("\nsh: cd: %s: %s", argv[1], strerror(errno));
		}

		printf("\n");
		return;
	}
	else if(b_count == 4 && (strncmp(buffer, "pwd", 4) == 0))
	{
		char buf[PATH_MAX];
		if(!getcwd(buf, PATH_MAX))
			printf("\ncwd: failed to get working directory: %s", strerror(errno));
		
		printf("\n%s\n", buf);
		return;
	}
	else if(b_count == 5 && (strncmp(buffer, "exit", 5) == 0)) 
		exit(0);	
	else
	{
		printf("\n");
		pid_t s_pid;
		pid_t save_pgid = getpgid(0);
		s_pid = spawnp(argv, NULL, SPAWN_SETPGID);
		if(s_pid < 0)
		{
			printf("sh: %s: %s\n", argv[0], strerror(errno));
			return;
		}

		tcsetpgrp(STDIN_FILENO, s_pid);
		int p_status;
		wait(&p_status);
		tcsetpgrp(STDIN_FILENO, save_pgid);
		if(WIFSIGNALED(p_status))
			printf("%s\n", strsignal(WTERMSIG(p_status)));
		return;
	}
	
	printf("\nsh: %s: command not found\n", buffer);
}

int main()
{
	struct termios tm;
	tcgetattr(STDIN_FILENO, &tm);
	tm.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, 0, &tm);

	printf("[root@monolith]# ");
	fflush(stdout);

	b_count = 0;
	int running = 1;
	while(running)
	{
		ssize_t count = read(STDIN_FILENO, input_buf, 256);

		for(ssize_t i = 0; i < count; i++)
		{
			char data = input_buf[i];
			if(data >= 32 && data <= 126 && b_count < 4095)
				buffer[b_count++] = data;

			if(data == 0x7f && b_count)
			{
				buffer[b_count] = '\0';
				b_count--;
			}

			if(data == '\n')
			{
				if(b_count > 0)
				{
					buffer[b_count++] = '\0';
					execute();
				}
				else
				{
					printf("\n");
				}

				b_count = 0;
				printf("[root@monolith]# ");
				fflush(stdout);
				break;
			}

			write(STDOUT_FILENO, &data, 1);
		}
	}

	return 0;
}
