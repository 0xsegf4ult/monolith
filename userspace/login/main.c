#include <stdio.h>
#include <sys/wait.h>
#include <sys/spawn.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	if(argc < 2)
		return 1;

	printf("logged in as %s on /dev/tty1\n", argv[1]);

	const char* s_argv[2] = {"/bin/sh", NULL};
	spawn(s_argv, NULL, 0);
	wait(NULL);
	return 0;
}
