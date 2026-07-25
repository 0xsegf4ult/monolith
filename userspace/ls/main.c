#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>



static char buffer[1024];

int main(int argc, char* argv[])
{
	int fd;
	int opt;
	int show_hidden = 0;
	int show_long = 0;

	const char* opt_bp = "al";
	while((opt = getopt(argc, argv, opt_bp)) != -1)
	{
		switch(opt)
		{
		case 'a':
			show_hidden = 1;
			break;
		case 'l':
			show_long = 1;
			break;
		default:
			break;
		}
	}

	if(optind < argc)
		fd = open(argv[optind], 0);
	else
		fd = open(".", 0);

	if(fd < 0)
	{
		printf("ls: cannot access %s: %s", optind < argc ? argv[optind] : ".", strerror(errno));
		return 1;
	}

	ssize_t read_count = posix_getdents(fd, buffer, 1024, 0);
	close(fd);
	if(read_count < 0)
	{
		printf("ls: %s: %s", optind <  argc ? argv[optind] : ".", strerror(errno));
		return 1;
	}

	if(read_count == 0)
		return 0;

	size_t bpos = 0;
	int first = 1;
	while(bpos < read_count)
	{
		struct posix_dent* d = (struct posix_dent*)(buffer + bpos);

		const char* name = d->d_name;

		if(name[0] != '.' || show_hidden > 0)
		{
			if(show_long)
			{
				if(!first)
					printf("\n");

				char pbuf[64];
				sprintf(pbuf, "%s/%s", optind < argc ? argv[optind] : ".", name);
				struct stat f_stat = {};
				int st_r = stat(pbuf, &f_stat);

				mode_t mode = f_stat.st_mode;

				char type = '-';
				if(S_ISDIR(mode))
					type = 'd';
				else if(S_ISCHR(mode))
					type = 'c';
				else if(S_ISBLK(mode))
					type = 'b';
				else if(S_ISLNK(mode))
					type = 'l';

				printf("%c%c%c%c%c%c%c%c%c%c %d root root %d %s", type,
					mode & S_IRUSR ? 'r' : '-',
					mode & S_IWUSR ? 'w' : '-',
					mode & S_IXUSR ? 'x' : '-',
					mode & S_IRGRP ? 'r' : '-',
					mode & S_IWGRP ? 'w' : '-',
					mode & S_IXGRP ? 'x' : '-',
					mode & S_IROTH ? 'r' : '-',
					mode & S_IWOTH ? 'w' : '-',
					mode & S_IXOTH ? 'x' : '-',
					f_stat.st_nlink,
					f_stat.st_size, name);
			}
			else
			{
				printf(first ? "%s" : " %s", name);
			}

			first = 0;
		}

		bpos += d->d_reclen;
	}

	printf("\n");

	return 0;
}
