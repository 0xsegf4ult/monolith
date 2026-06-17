#include <string.h>
#include <stdio.h>

char* optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

static int optpos = 1;

int getopt(int argc, char* const argv[], const char* optstring)
{
	optarg = NULL;
	
	if(optind >= argc || argv[optind] == NULL || argv[optind][0] != '-' || argv[optind][1] == '\0')
		return -1;

	if(!strcmp(argv[optind], "--"))
	{
		optind++;
		return -1;
	}

	char opt = argv[optind][optpos];
	const char* oli = strchr(optstring, opt);

	if(oli == NULL || opt == ':')
	{
		optopt = opt;
		if(opterr && optstring[0] != ':')
			printf("%s: unknown option -- %c\n", argv[0], opt);

		if(argv[optind][++optpos] == '\0')
		{
			optind++;
			optpos = 1;
		}

		return '?';
	}

	if (oli[1] == ':')
	{
		if(argv[optind][optpos + 1] != '\0')
		{
			optarg = &argv[optind][optpos = 1];
			optind++;
			optpos = 1;
		}
		else
		{
			optind++;
			if(optind >= argc)
			{
				optopt = opt;
				if(opterr && optstring[0] != ':')
					printf("%s: option requires an argument -- %c\n", argv[0], opt);

				optpos = 1;
				return (optstring[0] == ':') ? ':' : '?';
			}
			optarg = argv[optind];
			optind++;
			optpos = 1;
		}
	}
	else
	{
		if(argv[optind][++optpos] == '\0')
		{
			optind++;
			optpos = 1;
		}
	}

	return opt;
}
