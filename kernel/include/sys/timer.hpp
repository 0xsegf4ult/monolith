#pragma once

#include <list.hpp>
#include <stdint.h>

struct timer_device
{
	const char* name;
	uint8_t priority;
	
	uint8_t shift;
	uint64_t mult;

	void (*set_periodic)(timer_device*);

	list_node_t list_node;
};

void timer_interrupt();
void timer_start();
void timer_device_register(timer_device& device);
