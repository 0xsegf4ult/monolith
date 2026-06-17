#include <stdio.h>
#include <unistd.h>

int main()
{
	char path[256];

	int status = getcwd(path, 256);
	if(status < 0)
		return status;

	printf("%s\n", path);

	return 0;
}
