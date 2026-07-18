#pragma once

#include <mm/memory_map.h>
#include <types.h>

typedef struct 
{
	memory_map_t memory_map;
	physaddr_t kload_addr;
	virtaddr_t initramfs_address;
	size_t initramfs_size;
	virtaddr_t rsdp_address;
} boot_info_t;

extern boot_info_t boot_info;

void kernel_main(); 
