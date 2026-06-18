#ifndef _LIBC_TERMIOS_H
#define _LIBC_TERMIOS_H

#include <stdint.h>

#define NCCS 11

typedef unsigned char cc_t;
typedef uint32_t speed_t;
typedef uint32_t tcflag_t;

struct termios
{
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
};

struct winsize
{
	uint16_t ws_row;
	uint16_t ws_col;
	uint16_t ws_xpixel;
	uint16_t ws_ypixel;
};

#define IGNBRK          0000001
#define BRKINT          0000002
#define IGNPAR          0000004
#define PARMRK          0000010
#define INPCK           0000020
#define ISTRIP          0000040
#define INLCR           0000100
#define IGNCR           0000200
#define ICRNL           0000400
#define IUCLC           0001000
#define IXON            0002000
#define IXANY           0004000
#define IXOFF           0010000
#define IMAXBEL         0020000
#define IUTF8           0040000

#define OPOST           0000001
#define OLCUC           0000002
#define ONLCR           0000004
#define OCRNL           0000010
#define ONOCR           0000020
#define ONLRET          0000040
#define OFILL           0000100
#define OFDEL           0000200

#define CSIZE           0000060
#define CS5             0000000
#define CS6             0000020
#define CS7             0000040
#define CS8             0000060
#define CSTOPB          0000100
#define CREAD           0000200
#define PARENB          0000400
#define PARODD          0001000
#define HUPCL           0002000
#define CLOCAL          0004000

#define ISIG		0000001
#define ICANON		0000002
#define ECHO		0000010
#define ECHOE		0000020
#define ECHOK		0000040
#define ECHONL		0000100
#define ECHOCTL		0001000
#define NOFLSH		0000200
#define TOSTOP		0000400
#define IEXTEN		0100000

#define VINTR		0
#define VQUIT		1
#define VERASE		2
#define VKILL		3
#define VEOF		4
#define	VTIME		5
#define VMIN		6
#define VSWTC		7
#define VSTART		8
#define VSTOP		9
#define VSUSP		10
#define VEOL		11

#define B38400 		0000017

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, const struct termios* termios_p);
int tcgetwinsize(int, struct winsize* winsize_p);

#endif
