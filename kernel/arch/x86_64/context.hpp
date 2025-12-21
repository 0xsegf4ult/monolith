#pragma once

#include <lib/types.hpp>

struct __attribute__((packed)) cpu_context_t
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

	uint64_t interrupt_id;
	uint64_t error_code;

	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

extern "C" void arch_context_switch(virtaddr_t* old_rsp, virtaddr_t new_rsp);
extern "C" void arch_switch_to_usermode(virtaddr_t rsp, virtaddr_t rip);
