#pragma once

#include <libk/list.h>
#include <types.h>

enum clock_ids : clockid_t
{
	CLOCK_REALTIME = 0,
	CLOCK_MONOTONIC = 1,
	CLOCK_BOOTTIME = 7
};

typedef struct clock_source_type clocksource_t;

struct clock_source_type
{
	const char* name;
	uint8_t priority;

	uint64_t (*read)(clocksource_t* source);
	void (*enable)(clocksource_t* source);
	void (*disable)(clocksource_t* source);

	list_node_t list_node;
};

void clocksource_register(clocksource_t* source);
uint64_t clock_uptime();
void clock_wait(uint64_t nanos);
void clock_set_boottime(time_t boottime);

int sys_clock_gettime(clockid_t clock, struct timespec* tv);
