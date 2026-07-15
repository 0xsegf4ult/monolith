#pragma once

#include <types.hpp>

struct task_t;
virtaddr_t elf_exec(int execfd, task_t* task);
