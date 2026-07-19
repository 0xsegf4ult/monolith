#include <init.h>

#include <dev/console.h>
#include <dev/efifb.h>

#include <fs/initramfs.h>
#include <fs/vfs.h>
#include <mm/mmu.h>
#include <mm/vmm.h>

#include <libk/string.h>

#include <klog.h>
#include <panic.h>

boot_info_t boot_info;

void kernel_main()
{
	vfs_init();
	vfs_mkdir("/dev", 0755);

	efifb_init(&boot_info.fb);
	console_init();

	mmu_map_range(vm_get_kernel_space()->mmu_root, boot_info.initramfs_address - VM_DMAP_BASE, boot_info.initramfs_address, boot_info.initramfs_size, PROT_READ, 0);
	initramfs_unpack((byte*)boot_info.initramfs_address, boot_info.initramfs_size);	
}
