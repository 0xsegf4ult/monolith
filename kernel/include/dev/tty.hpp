#pragma once

#include <types.hpp>

constexpr size_t NCCS = 11;
using tcflag_t = uint32_t;
using cc_t = unsigned char;

enum tty_iflags : tcflag_t
{
	IGNBRK 		= 0000001,
	BRKINT		= 0000002,
	IGNPAR		= 0000004,
	PARMRK		= 0000010,
	INPCK		= 0000020,
	ISTRIP		= 0000040,
	INLCR		= 0000100,
	IGNCR		= 0000200,
	ICRNL		= 0000400,
	IUCLC		= 0001000,
	IXON		= 0002000,
	IXANY		= 0004000,
	IXOFF		= 0010000,
	IMAXBEL		= 0020000,
	IUTF8		= 0040000
};

enum tty_oflags : tcflag_t
{
	OPOST		= 0000001,
	OLCUC		= 0000002,
	ONLCR		= 0000004,
	OCRNL		= 0000010,
	ONOCR		= 0000020,
	ONLRET		= 0000040,
	OFILL		= 0000100,
	OFDEL		= 0000200
};

enum tty_cflags : tcflag_t
{
	CSIZE		= 0000060,
	CS5		= 0000000,
	CS6		= 0000020,
	CS7		= 0000040,
	CS8		= 0000060,
	CSTOPB		= 0000100,
	CREAD		= 0000200,
	PARENB		= 0000400,
	PARODD		= 0001000,
	HUPCL		= 0002000,
	CLOCAL		= 0004000
};

enum tty_lflags : tcflag_t
{
	ISIG		= 0000001,
	ICANON		= 0000002,
	ECHO		= 0000010,
	ECHOE		= 0000020,
	ECHOK		= 0000040,
	ECHONL		= 0000100,
	ECHOCTL		= 0001000,
	NOFLSH		= 0000200,
	TOSTOP		= 0000400,
	IEXTEN		= 0100000
};

enum tty_cc : cc_t
{
	VINTR = 0,
	VQUIT = 1,
	VERASE = 2,
	VKILL = 3,
	VEOF = 4,
	VTIME = 5,
	VMIN = 6,
	VSWTC = 7,
	VSTART = 8,
	VSTOP = 9,
	VSUSP = 10,
	VEOL = 11
};

constexpr cc_t B38400 = 0000017;

struct termios_t
{
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
};

struct winsize_t
{
	uint16_t ws_row;
	uint16_t ws_col;
	uint16_t ws_xpixel;
	uint16_t ws_ypixel;
};

enum tty_io
{
	__TIO 		= 90,
	TCGETS		= (__TIO << 8) + 3,
	TCSETS		= (__TIO << 8) + 4,
	TIOCGWINSZ	= (__TIO << 8) + 8
};

struct tty_device;

void tty_init();
void tty_consume(tty_device* tty, char c);
ssize_t tty_read(tty_device* tty, byte* buffer, size_t length);
ssize_t tty_write(tty_device* tty, const byte* buffer, size_t length);
