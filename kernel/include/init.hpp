#pragma once

#include <mm/memory_map.hpp>
#include <dev/efifb.hpp>
#include <types.hpp>

struct boot_info_t
{
	mm::memory_map memmap;
	physaddr_t phys_kernel_start;
	virtaddr_t initramfs_address;
	size_t initramfs_size;
	virtaddr_t rsdp_address;
	efifb_framebuffer fb;
};

extern boot_info_t boot_info;

void kernel_main(); 
