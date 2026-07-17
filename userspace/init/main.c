#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/spawn.h>
#include <unistd.h>

int main()
{
	if(getpid() != 1)
	{
		fprintf(stderr, "init: must be run as PID 1\n");
		return 0;
	}
	
	fprintf(stderr, "  monolith x86_64 0.01.2 is starting up\n\n");
        fprintf(stderr, "* /proc is already mounted\n");

	const char* argv[3] = {"/bin/getty", "/dev/tty1", NULL};
	bool respawn = true;
	pid_t gpid = 0;
	for(;;)
	{
		if(respawn)
		{
			respawn = false;
			fprintf(stderr, "* Starting %s on %s...", argv[0], argv[1]);
			gpid = spawn(argv, NULL, 0);
			if(gpid < 0)
				fprintf(stderr, "...[FAILED]");
			fprintf(stderr, "\n");
		}

		pid_t wpid = wait(NULL);
		if(wpid == gpid)
		       respawn = true;	
	}
}
