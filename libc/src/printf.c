#include <stdio.h>
#include <syscall.h>
#include <string.h>
#include <stdarg.h>

static const char *digits= "0123456789abcdef";

char* num_to_str(char* buf, int32_t num, int32_t base, uint8_t flags)
{
	if(base < 2 || base > 16)
		return buf;

	size_t len = 0;
	char tmp_str[32];

	int32_t negative_num = num < 0 ? num : -num;

	do
	{
		tmp_str[len++] = digits[-(negative_num % base)];
		negative_num /= base;
	} while(negative_num);

	if(num < 0 && (flags & 1))
		tmp_str[len++] = '-';

	for(int i = len - 1; i >= 0; i--)
		*(buf++) = tmp_str[i];

	return buf;
}

char* ptr_to_str(char* buf, uintptr_t ptr)
{
	*(buf++) = '0';
	*(buf++) = 'x';

	size_t len = 0;
	char tmp_str[32];

	do
	{
		tmp_str[len++] = digits[ptr % 16];
		ptr /= 16;
	} while(ptr);

	for(int i = len - 1; i >= 0; i--)
		*(buf++) = tmp_str[i];

	return buf;
}

int vsprintf(char* buf, const char* fmt, va_list args)
{
	char* original = buf;

	while(*fmt)
	{
		if(*fmt != '%')
		{
			*(buf++) = *(fmt++);
			continue;
		}
		
		fmt++;

		int flags = 0;
		char* str;
		int len = 0;

		switch(*fmt)
		{
		case '%':
			*(buf++) = '%';
			break;
		case 'c':
			*(buf++) = (unsigned char) va_arg(args, int);
			break;
		case 's':
			str = va_arg(args, char*);
			len = strlen(str);

			for(int i = 0; i < len; i++)
				*(buf++) = *(str++);

			break;
		case 'd':
		case 'i':
			flags |= 1;
		case 'u':
			buf = num_to_str(buf, va_arg(args, int32_t), 10, flags);
			break;
		case 'p':
			buf = ptr_to_str(buf, va_arg(args, uintptr_t));
			break;
		case 'x':
			buf = num_to_str(buf, va_arg(args, uint32_t), 16, flags);
			break;
		}

		fmt++;
	}

	return buf - original;
}

static char buffer[1024];

int printf(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int i = vsprintf(buffer, fmt, args);
	va_end(args);

	write(0, buffer, i);
	return i;
}
