#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
	if(getpid() != 1)
	{
		printf("init: must be run as PID 1");
		return 0;
	}

	if(open("/dev/null", 0) < 0)
		return 1;

	if(open("/dev/console", 0) < 0)
		return 1;

	if(open("/dev/console", 0) < 0)
		return 1;

	printf("  monolith x86_64 0.01.2 is starting up\n\n");
        printf("* /proc is already mounted\n\n");

	const char* argv[3] = {"/bin/getty", "/dev/tty1", NULL};
	bool respawn = true;
	pid_t gpid = 0;
	for(;;)
	{
		if(respawn)
		{
			respawn = false;
			spawn(&gpid, argv, 0);
		}

		pid_t wpid = wait(NULL);
		if(wpid == gpid)
		       respawn = true;	
	}
}
