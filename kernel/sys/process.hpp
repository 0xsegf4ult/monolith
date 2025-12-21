#pragma once

#include <arch/x86_64/context.hpp>
#include <lib/types.hpp>

enum class process_status
{
	ready,
	running,
	dead
};

struct address_space;

struct process_t
{
	char name[32];
	uint32_t pid;
	process_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	address_space* vm_space;
	process_t* next;
};

process_t* create_process(const char* name, virtaddr_t entry, void* arg);

