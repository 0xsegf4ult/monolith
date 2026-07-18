#pragma once

#include <stdint.h>

enum ioapic_address_t : uint8_t
{
	IOAPIC_ADDRESS_REGISTER = 0x00,
	IOAPIC_ADDRESS_DATA	= 0x10
};

enum ioapic_register_t : uint8_t
{
	IOAPIC_REGISTER_ID	= 0x00,
	IOAPIC_REGISTER_VER	= 0x01,
	IOAPIC_REGISTER_ARB	= 0x02,
	IOAPIC_REGISTER_REDTBL	= 0x10
};

enum ioapic_delivery_mode_t : uint8_t
{
	IOAPIC_DELIVERY_MODE_FIXED 	= 0b000,
	IOAPIC_DELIVERY_MODE_LOWPRIO	= 0b001,
	IOAPIC_DELIVERY_MODE_SMI	= 0b010,
	IOAPIC_DELIVERY_MODE_NMI	= 0b100,
	IOAPIC_DELIVERY_MODE_INIT	= 0b101,
	IOAPIC_DELIVERY_MODE_EXTINT	= 0b111
};

enum ioapic_destination_mode_t : uint8_t
{
	IOAPIC_DESTINATION_PHYSICAL 	= 0,
	IOAPIC_DESTINATION_LOGICAL 	= 1
};

enum ioapic_pin_polarity_t : uint8_t
{
	IOAPIC_POLARITY_ACTIVE_HIGH 	= 0,
	IOAPIC_POLARITY_ACTIVE_LOW	= 1,
};

enum ioapic_trigger_mode_t : uint8_t
{
	IOAPIC_TRIGGER_MODE_EDGE	= 0,
	IOAPIC_TRIGGER_MODE_LEVEL	= 1
};

typedef union 
{
	struct __attribute__((packed))
	{
		uint8_t vector;
		uint8_t delivery_mode : 3;
		uint8_t destination_mode : 1;
		uint8_t delivery_status : 1;
		uint8_t polarity : 1;
		uint8_t remote_irr : 1;
		uint8_t trigger_mode : 1;
		uint8_t mask : 1;
		uint64_t reserved: 39;
		uint8_t destination;
	};

	uint64_t raw;
} ioapic_redir_entry_t;

void ioapic_init();
