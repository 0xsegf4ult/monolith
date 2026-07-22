#pragma once

#include <sched/waitqueue.h>
#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <types.h>

constexpr size_t NCCS = 32;
typedef uint32_t tcflag_t;
typedef unsigned char cc_t;
typedef uint32_t speed_t;

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

struct termios
{
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
	speed_t ibaud;
	speed_t obaud;
};

struct winsize
{
	uint16_t ws_row;
	uint16_t ws_col;
	uint16_t ws_xpixel;
	uint16_t ws_ypixel;
};

enum tty_io
{
	TCGETS		= 0x5401,
	TCSETS		= 0x5402,
	TIOCSPGRP	= 0x5410,
	TIOCGWINSZ	= 0x5413
};

typedef void (*tty_output_t)(const char*, size_t);

constexpr size_t tty_buffer_size = 0x1000;

struct tty_device
{
	byte* read_buffer;
        uint32_t read_buffer_head;
        uint32_t read_buffer_tail;
        
	byte* write_buffer;
	uint32_t write_buffer_head;
	uint32_t write_buffer_tail;

	spinlock_t write_buffer_lock;

        tty_output_t output;

        pid_t session_id;
        pid_t fg_pgrp;

        wait_queue waitqueue;
        mutex_t lock;

        struct termios termios;
        struct winsize winsize;
};

struct tty_device* tty_create(uint16_t index, tty_output_t output_fn);
void tty_consume(struct tty_device* tty, char c);
ssize_t tty_read(struct tty_device* tty, byte* buffer, size_t length);
ssize_t tty_write(struct tty_device* tty, const byte* buffer, size_t length);
