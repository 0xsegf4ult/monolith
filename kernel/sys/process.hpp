#pragma once

#include <arch/x86_64/context.hpp>
#include <lib/types.hpp>

enum class process_status
{
	ready,
	running,
	sleeping,
	terminated	
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
	virtaddr_t entry;
	int open_files[32];
	process_t* next;
};

process_t* create_process(const char* name, bool is_user);

