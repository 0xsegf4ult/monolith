#include <ctype.h>

int isdigit(char ch)
{
	if(ch >= '0' && ch <= '9')
		return ch;

	return 0;
}
