#pragma once

#include <libk/list.h>
#include <stdint.h>

typedef struct timer_device_type timer_device;

struct timer_device_type
{
	const char* name;
	uint8_t priority;
	
	uint8_t shift;
	uint64_t mult;

	void (*set_periodic)(timer_device*);
	void (*eoi)();

	list_node_t list_node;
};

void timer_interrupt();
void timer_start();
void timer_device_register(timer_device* device);
