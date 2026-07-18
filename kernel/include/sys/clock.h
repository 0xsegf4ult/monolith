#pragma once

#include <libk/list.h>
#include <stdint.h>

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
