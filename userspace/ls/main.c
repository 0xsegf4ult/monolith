#include <stdio.h>
#include <syscall.h>
#include <string.h>

static char buffer[1024];

int main(int argc, const char** argv)
{
	int fd;
	int show_hidden = 0;
	if(argc == 2)
		fd = open(argv[1], 0);
	else
		fd = open(".", 0);

	if(fd < 0)
	{
		printf("ls: cannot access %s: %s", argc == 2 ? argv[1] : ".", strerrordesc_np(-fd));
		return fd;
	}

	ssize_t read_count = getdents(fd, buffer, 1024);
	close(fd);
	if(read_count < 0)
		return read_count;

	if(read_count == 0)
		return 0;

	size_t bpos = 0;
	int first = 1;
	while(bpos < read_count)
	{
		dirent_info* d = (dirent_info*)(buffer + bpos);
		const char* name = (const char*)d + sizeof(dirent_info);

		if(name[0] != '.' || show_hidden > 0)
			printf(first ? "%s" : " %s", (const char*)d + sizeof(dirent_info));
		
		first = 0;

		bpos += d->length;
	}

	return 0;
}
