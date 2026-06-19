#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("stat: missing file operand");
		return 0;
	}

	struct stat f_stat;
	int st_r = stat(argv[1], &f_stat);
	if(st_r < 0)
	{
		printf("stat: cannot stat %s: %s", argv[1], strerrordesc_np(-st_r));
		return -1;
	}

	mode_t mode = f_stat.mode;
	char type = '-';
	const char* typestr = "regular file";
	if(S_ISDIR(mode))
	{
		typestr = "directory";
		type = 'd';
	}
	else if(S_ISCHR(mode))
	{
		typestr = "character special file";
		type = 'c';
	}
	else if(S_ISBLK(mode))
	{
		typestr = "block special file";
		type = 'b';
	}
	else if(S_ISLNK(mode))
	{
		typestr = "symbolic link";
		type = 'l';
	}

	printf("File: %s\n"
	       "Size: %d %s\n"
	       "Links: %d\n"
	       "Access: (0%o/%c%c%c%c%c%c%c%c%c%c) Uid: (%d) Gid: (%d)\n",
		argv[1],
		f_stat.size,
		typestr,
		f_stat.nlinks,
		mode & 0777,
	       	type,
		mode & S_IRUSR ? 'r' : '-',
		mode & S_IWUSR ? 'w' : '-',
		mode & S_IXUSR ? 'x' : '-',
		mode & S_IRGRP ? 'r' : '-',
		mode & S_IWGRP ? 'w' : '-',
		mode & S_IXGRP ? 'x' : '-',
		mode & S_IROTH ? 'r' : '-',
		mode & S_IWOTH ? 'w' : '-',
		mode & S_IXOTH ? 'x' : '-',
		0, 0);

	return 0;
}
