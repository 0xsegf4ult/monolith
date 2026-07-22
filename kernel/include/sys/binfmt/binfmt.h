#pragma once

#include <libk/list.h>
#include <types.h>

struct task;
typedef int (*binfmt_exec_t)(int, struct task*, virtaddr_t*);

constexpr size_t BINFMT_SIGMAX = 15;

typedef struct
{
	const char* name;
	binfmt_exec_t exec;
	list_node_t list_node;
	uint8_t signature[BINFMT_SIGMAX];
	uint8_t siglen;
} binfmt_descriptor_t;

void binfmt_register(binfmt_descriptor_t* desc);
void binfmt_init();
void binfmt_exec_task();
