#include <libk/vsprintf.h>
#include <libk/string.h>
#include <types.h>

enum PRINT_FLAGS
{
	FLAG_SIGNED = 1,
	FLAG_ZERO = 2,
};

static const char* digits = "0123456789abcdef";

char* num_to_str(char* buf, int64_t num, int32_t base, int flags, int padding)
{
	if(base < 2 || base > 16)
		return buf;

	ssize_t len = 0;
	char tmp_str[64];

	int64_t negative_num = num < 0 ? num : -num;

	do
	{
		tmp_str[len++] = digits[-(negative_num % base)];
		negative_num /= base;
	} while(negative_num);

	if(num < 0 && (flags & FLAG_SIGNED))
		tmp_str[len++] = '-';

	padding -= len;

	while(padding-- > 0)
		*(buf++) = (flags & FLAG_ZERO) ? '0' : ' ';

	for(ssize_t i = len - 1; i >= 0; i--)
		*(buf++) = tmp_str[i];

	return buf;
}

char* ptr_to_str(char* buf, uintptr_t ptr, int flags, int padding)
{
	(*buf++) = '0';
	(*buf++) = 'x';

        ssize_t len = 0;
        char tmp_str[32];

        do
        {
                tmp_str[len++] = digits[ptr % 16];
                ptr /= 16;
        } while(ptr);
	
	padding -= len;
	
	while(padding-- > 0)
		*(buf++) = (flags & FLAG_ZERO) ? '0' : ' ';

        for(ssize_t i = len - 1; i >= 0; i--)
                *(buf++) = tmp_str[i];

        return buf;
}

ssize_t vsprintf(char* buf, const char* fmt, va_list args)
{
	char* orig = buf;

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
		ssize_t len = 0;
		char length = '\0';
		int32_t base = 10;
		int width = 0;

		if(*fmt == '0')
		{
			flags |= FLAG_ZERO;
			fmt++;
		}

		while(*fmt >= '0' && *fmt <= '9')
		{
			width *= 10;
			width += *fmt - 48;
			fmt++;
		}

		if(*fmt == 'l' || *fmt == 'z')
		{
			length = *fmt;
			fmt++;
		}

		char specifier = *fmt;

		switch(specifier)
		{
		case 'd':
		case 'i':
			flags |= FLAG_SIGNED;
			specifier = 'u';
			break;
		case 'x':
			base = 16;
			specifier = 'u';
			break;
		case 'o':
			base = 8;
			specifier = 'u';
			break;
		default:
			break;
		}

		switch(specifier)
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

			for(ssize_t i = 0; i < len; i++)
				*(buf++) = *(str++);

			break;
		case 'u':
			switch(length)
			{
			case 0:
				buf = num_to_str(buf, va_arg(args, int32_t), base, flags, width);
				break;
			case 'l':
				buf = num_to_str(buf, va_arg(args, int64_t), base, flags, width);
				break;
			case 'z':
				buf = num_to_str(buf, va_arg(args, size_t), base, flags, width);
				break;
			default:
				break;
			}
			break;
		case 'p':
			buf = ptr_to_str(buf, va_arg(args, uintptr_t), flags, width);
			break;
		}

		fmt++;
	}
	*(buf++) = '\0';

	return (ssize_t)(buf - orig);
}

ssize_t sprintf(char* buf, const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ssize_t i = vsprintf(buf, fmt, args);
	va_end(args);

	return i;
}
