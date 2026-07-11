#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	if(argc < 2)
		return 1;

	close(0);
	close(1);
	close(2);

	setsid();

	if(open(argv[1], 0) < 0)
		return 1;

	if(open(argv[1], 0) < 0)
		return 1;

	if(open(argv[1], 0) < 0)
		return 1;

	const char* s_argv[3] = {"/bin/login", "root", NULL};
	spawn(NULL, s_argv, 0);
	wait(NULL);
	return 0;
}
