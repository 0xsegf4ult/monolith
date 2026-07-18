#pragma once

#include <stdint.h>

enum interrupt_vector_t
{
	INTERRUPT_VECTOR_DIVISION_ERROR		= 0x00,
	INTERRUPT_VECTOR_DEBUG			= 0x01,
	INTERRUPT_VECTOR_NMI			= 0x02,
	INTERRUPT_VECTOR_BREAKPOINT		= 0x03,
	INTERRUPT_VECTOR_OVERFLOW		= 0x04,
	INTERRUPT_VECTOR_BOUNDS_EXCEEDED	= 0x05,
	INTERRUPT_VECTOR_INVALID_OPCODE		= 0x06,
	INTERRUPT_VECTOR_DEVICE_UNAVAILABLE	= 0x07,
	INTERRUPT_VECTOR_DOUBLE_FAULT		= 0x08,
	INTERRUPT_VECTOR_FPU_SEGMENT		= 0x09,
	INTERRUPT_VECTOR_INVALID_TSS		= 0x0A,
	INTERRUPT_VECTOR_SEGMENT_NOT_PRESENT	= 0x0B,
	INTERRUPT_VECTOR_STACK_SEGMENT_FAULT	= 0x0C,
	INTERRUPT_VECTOR_GPF			= 0x0D,
	INTERRUPT_VECTOR_PAGE_FAULT		= 0x0E,
	INTERRUPT_VECTOR_RESERVED		= 0x0F,
	INTERRUPT_VECTOR_FPU_EXCEPTION		= 0x10,
	INTERRUPT_VECTOR_ALIGNMENT_CHECK	= 0x11,
	INTERRUPT_VECTOR_MACHINE_CHECK		= 0x12,
	INTERRUPT_VECTOR_SIMD_EXCEPTION		= 0x13,
	INTERRUPT_VECTOR_VIRT_EXCEPTION		= 0x14,
	INTERRUPT_VECTOR_CONTROLFLOW_EXCEPTION	= 0x15,
	INTERRUPT_VECTOR_HYPERVISOR_EXCEPTION	= 0x1C,
	INTERRUPT_VECTOR_VMM_EXCEPTION		= 0x1D,
	INTERRUPT_VECTOR_SECURITY_EXCEPTION	= 0x1E,
	INTERRUPT_VECTOR_EXCEPTION_END 		= 0x20,
	INTERRUPT_VECTOR_HW_START 		= 0x20,
	INTERRUPT_VECTOR_HW_END			= 0x80,
	INTERRUPT_VECTOR_SYSCALL		= 0x80,
	
	INTERRUPT_VECTOR_TIMER			= 0xF0,
	INTERRUPT_VECTOR_SPURIOUS		= 0xFF
};

typedef struct
{
	uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;
        uint64_t rbp;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
	uint64_t rax;

	uint64_t vector;
	uint64_t error_code;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} interrupt_frame;

static inline void local_irq_disable()
{
	asm volatile("cli" ::: "memory");
}

static inline void local_irq_enable()
{
	asm volatile("sti" ::: "memory");
}

static inline uint64_t local_irq_save()
{
	uint64_t flags;
	asm volatile("pushfq; popq %0" : "=rm"(flags) :: "memory");
	local_irq_disable();
	return flags;
}

static inline void local_irq_restore(uint64_t flags)
{
	asm volatile("pushq %0; popfq" :: "g"(flags) : "memory", "cc");
}
