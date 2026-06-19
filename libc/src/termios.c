#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define __TIO 90
#define TCGETS ((__TIO << 8) + 3)
#define TCSETS ((__TIO << 8) + 4)
#define TIOCGWINSZ ((__TIO << 8) + 8)

int tcgetattr(int fd, struct termios* termios_p)
{
	return ioctl(fd, TCGETS, (uint64_t)termios_p);
}

int tcsetattr(int fd, const struct termios* termios_p)
{
	return ioctl(fd, TCSETS, (uint64_t)termios_p);
}

int tcgetwinsize(int fd, struct winsize* winsize_p)
{
	return ioctl(fd, TIOCGWINSZ, (uint64_t)winsize_p);
}
