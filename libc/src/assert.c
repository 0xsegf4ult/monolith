#include <stdio.h>
#include <stdlib.h>

void __assert(int expr, const char* expr_str, const char* file, int line, const char* func)
{
	if(expr)
		return;

	printf("%s:%d: %s: Assertion '%s' failed.\n", file, line, func, expr_str);
	abort();
}
