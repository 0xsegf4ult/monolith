#pragma once

#include <types.h>

enum GDT_RING 
{
	GDT_RING0 = 0,
	GDT_RING3 = 3
};

enum GDT_DESCRIPTOR : uint32_t 
{
	GDT_NULL 	= 0x00,
	GDT_KERNEL_CS	= 0x08,
	GDT_KERNEL_DS	= 0x10,
	GDT_USER_CS	= 0x18,
	GDT_USER_DS	= 0x20,
	GDT_TSS		= 0x28
};

enum GDT_ACCESS : uint8_t
{
	GDT_ACCESSED 	= 0b00000001,
	GDT_READ	= 0b00000010,
	GDT_WRITE	= 0b00000010,
	GDT_GROWSDOWN 	= 0b00000100,
	GDT_CONFORMING	= 0b00000100,
	GDT_EXEC	= 0b00001000,
	GDT_SYSTEM	= 0b00000000,
	GDT_TSS64_AVL	= 0b00001001,
	GDT_DATA	= 0b00010000,
	GDT_CODE	= 0b00010000,
	GDT_PRIV_RING0  = 0b00000000,
	GDT_PRIV_RING3	= 0b01100000,
	GDT_PRESENT	= 0b10000000
};

enum GDT_FLAGS : uint8_t
{
	GDT_FLAG_NONE		= 0b00000000,
	GDT_FLAG_RESERVED	= 0b00000001,
	GDT_FLAG_LONG_MODE 	= 0b00000010,
	GDT_FLAG_SIZE32		= 0b00000100,
	GDT_FLAG_SIZE64		= 0b00000000,
	GDT_FLAG_4K		= 0b00001000
};

typedef struct __attribute__((packed))
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t flags_limithigh;
	uint8_t base_high;
} gdt_segment_t;

typedef struct __attribute__((packed))
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_midL;
	uint8_t access;
	uint8_t flags_limithigh;
	uint8_t base_midU;
	uint32_t base_high;
	uint32_t reserved;
} gdt_system_segment_t;

typedef struct __attribute__((packed))
{
	uint16_t limit;
	virtaddr_t base;
} gdtr_t;

typedef struct __attribute__((packed))
{
	gdt_segment_t null;
	gdt_segment_t kernel_cs;
	gdt_segment_t kernel_ds;
	gdt_segment_t user_cs;
	gdt_segment_t user_ds;
	gdt_system_segment_t tss;
} gdt_t;

typedef struct __attribute__((packed))
{
	uint32_t reserved0;

	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;

	uint64_t reserved1;

	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;

	uint64_t reserved2;
	uint16_t reserved3;

	uint16_t iomap_base;
} tss_t;

struct cpu;
void gdt_init(struct cpu* cpu);
