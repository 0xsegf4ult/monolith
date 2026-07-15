#pragma once

#include <stdint.h>
#include <list.hpp>
#include <types.hpp>

struct task_t;
typedef virtaddr_t (*binfmt_exec_t)(int, task_t*);

constexpr size_t BINFMT_SIGMAX = 15;

struct binfmt_descriptor_t
{
	const char* name;
	binfmt_exec_t exec;
	list_node_t list_node;
	uint8_t signature[BINFMT_SIGMAX];
	uint8_t siglen;
};

void binfmt_register(binfmt_descriptor_t* desc);
void binfmt_init();
extern "C" void exec_task();
