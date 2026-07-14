#include <dev/rtc.hpp>
#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/io.hpp>
#include <sys/spinlock.hpp>

static uint16_t cmos_address_port = 0x70;
static uint16_t cmos_data_port = 0x71;
static uint8_t century_register = 0;
static spinlock_t lock;

//disables NMI
static uint8_t cmos_read(uint8_t reg)
{
	io::outb(reg | 0x80, cmos_address_port);
	return io::inb(cmos_data_port);
}

static bool rtc_update_in_progress()
{
	return(cmos_read(0x0a) & 0x80);
}

uint8_t bcd_to_bin(uint8_t bcd)
{
	return (bcd >> 4) * 10 + (bcd & 0xF);
}

static bool is_leap_year(int32_t y)
{
	return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static const int days_per_month[] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

time_t rtc_read()
{
	uint64_t rflags;
	spinlock_acquire_irqsave(lock, rflags);

	while(rtc_update_in_progress())
	{
		asm volatile("pause");
	}

	uint8_t seconds = bcd_to_bin(cmos_read(0x00));
	uint8_t minutes = bcd_to_bin(cmos_read(0x02));
	uint8_t hours = bcd_to_bin(cmos_read(0x04));
	uint8_t day = bcd_to_bin(cmos_read(0x07));
	uint8_t month = bcd_to_bin(cmos_read(0x08));
	uint8_t year = bcd_to_bin(cmos_read(0x09));

	uint16_t r_year;
	if(century_register)
	{
		uint8_t century = bcd_to_bin(cmos_read(century_register));
		r_year = century * 100 + year;
	}
	else
	{
		r_year = (year >= 70) ? (1900 + year) : (2000 + year);
	}
	
	spinlock_release_irqsave(lock, rflags);

	int64_t y_day = 0;
	for(int m = 1; m < month; m++)
	{
		if(m == 2 && is_leap_year(r_year))
			y_day += 29;
		else
			y_day += days_per_month[m - 1];
	}
	r_year -= 1900;
	y_day += (day - 1);

	time_t epoch = (((r_year + 299) / 400) * 86400) - (((r_year - 1) / 100) * 86400) + (((r_year - 69) / 4) * 86400) + ((r_year - 70) * 31536000) + y_day * 86400 + hours * 3600 + minutes * 60 + seconds;
	return epoch;
}

int rtc_init()
{
	spinlock_init(lock);

	auto* fadt = acpi_get_tables().fadt;
	if(fadt)
	{
		century_register = fadt->century;
	}

	return 0;
}
