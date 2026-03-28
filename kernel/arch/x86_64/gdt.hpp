#pragma once

#include <lib/types.hpp>

enum segment_selector
{
        KERNEL_CS = (1 << 3),
        KERNEL_DS = (2 << 3),
        USER_CS = (3 << 3),
        USER_DS = (4 << 3),
        TSS_SELECTOR = (5 << 3)
};

enum gdt_flags
{
        GDT_ACCESSED = 0b1,
        GDT_WRITABLE = 0b10,
        GDT_READABLE = 0b10,
        GDT_EXECUTABLE = 0b1000,
        GDT_TSS = 0b1001,
        GDT_CS = 0b10000,
        GDT_DS = 0b10000,
        GDT_PRESENT = 0b10000000
};

enum DPL
{
        DPL_KERNEL = 0x0,
        DPL_USER = 0x3
};

using gdt_entry_t = uint64_t;
struct __attribute__((packed)) gdtr_t
{
        uint16_t limit;
        uint64_t* base;
};
static_assert(sizeof(gdtr_t) == 10);

struct __attribute__((packed)) tss_t
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
};

struct cpu_t;
void setup_gdt(cpu_t* cpu);
