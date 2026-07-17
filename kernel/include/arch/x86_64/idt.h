#pragma once

#include <types.h>

enum : uint8_t
{
	IDT_PRESENT 	= 0b10000000,
	IDT_RING3	= 0b01100000,
	IDT_RING0	= 0b00000000,
	IDT_TRAP_GATE	= 0b00001111,
	IDT_INTR_GATE	= 0b00001110
};

typedef struct __attribute__((packed))
{
	uint16_t base16_low;
	uint16_t segment;
	uint8_t ist;
	uint8_t flags;
	uint16_t base16_high;
	uint32_t base32_high;
	uint32_t reserved;
} idt_entry_t;

typedef struct __attribute__((packed))
{
	uint16_t limit;
	virtaddr_t base;
} idtr_t;

void idt_setup();
void idt_load();
