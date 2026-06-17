#include <dev/nvme/nvme.hpp>
#include <dev/nvme/nvme_structs.hpp>

#include <arch/x86_64/interrupts.hpp>

#include <dev/pcie.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>
#include <dev/block.hpp>

#include <fs/vfs.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <lib/kstd.hpp>
#include <lib/klog.hpp>

static size_t nvme_device_id = 0;

struct nvme_queue
{
	void* address;
	physaddr_t phys_address;
	size_t size;
	size_t tail;

	size_t entry_size;

	bool is_full()
	{
		return (tail + 1) >= size;
	}

	void* allocate()
	{
		auto res = (void*)((byte*)address + tail * entry_size);
		tail = (tail + 1) % size;
		return res;
	}
};

struct nvme_device
{
	pcie_device pcie;
	virtaddr_t base_address;
	nvme_queue submission_queue;
	nvme_queue completion_queue;
	nvme_identify_controller* identify;

	size_t max_transfer;	
	size_t doorbell_stride;

	template <typename T>
	T read_register(uint32_t offset)
	{
		return *reinterpret_cast<const T*>(base_address + offset);
	}

	template <typename T>
	void write_register(uint32_t offset, T value)
	{
		log::debug("nvme_write offset {:#x}", offset);
		*reinterpret_cast<T*>(base_address + offset) = value;
	}
};

struct nvme_namespace
{
	size_t nsid;
	nvme_device* parent;
	nvme_identify_namespace* identify;
};

static bool intr_wait = false;
void nvme_irq_handler()
{
	log::debug("nvme0: interrupt");
	intr_wait = true;
}

void nvme_register_namespace(nvme_device* device, uint32_t ns)
{
	auto* blkdev = blockdev_alloc(dev_t{4, uint16_t(ns)});
	
	auto* nvme_ns = (nvme_namespace*)kmalloc(sizeof(nvme_namespace));
	nvme_ns->nsid = ns;
	nvme_ns->parent = device;
	
	blkdev->data = nvme_ns;

	auto dev_node = vfs::mknod("/dev/nvme0n1", S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dev_t{4, uint16_t(ns)});

	nvme_ns->identify = (nvme_identify_namespace*)vmalloc(0x1000, vm_write);

	nvme_cmd cmd{};
	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	cmd.nsid = ns;
	cmd.identify.cns = 0;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)nvme_ns->identify);
	cmd.prp2 = 0;
	
	auto* submission = (nvme_cmd*)device->submission_queue.allocate();
	auto* completion = (nvme_completion*)device->completion_queue.allocate();
	memcpy(submission, &cmd, sizeof(nvme_cmd));

	log::debug("nvme0n1: send identify command");
	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE, device->submission_queue.tail);

	while(true)
	{
		if(completion->phase_tag == 1)
			break;

		asm volatile("pause");
	}
	log::debug("nvme0n1: cmd completed with status {}", uint16_t(completion->status));

	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + 1 * (4 << device->doorbell_stride), device->completion_queue.tail);

	log::debug("nvme0n1: {:#x} blocks blk_size {:#x}", nvme_ns->identify->nsze, 1 << nvme_ns->identify->lbaf[nvme_ns->identify->flbas & 0xF].lba_data_size);

	uint64_t flba = nvme_ns->identify->flbas & 0xF;
	uint64_t lba_shift = nvme_ns->identify->lbaf[flba].lba_data_size;
	uint64_t max_lba = (device->max_transfer) / (1 << lba_shift);
}

void nvme_init_controller(pcie_device& dev)
{
	intr_wait = false;
	auto* chdev = chardev_alloc(dev_t{4, 0});

	auto* device = (nvme_device*)kmalloc(sizeof(nvme_device));
	device->pcie = dev;
	chdev->data = device;

	//FIXME: format
	auto dev_node = vfs::mknod("/dev/nvme0", S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dev_t{4, 0});

	auto pcie_reg1 = device->pcie.read_config32(0x4);
	pcie_reg1 |= ((0x400 | 0x4 | 0x2) & ~(0x1)); // INTERRUPT_DISABLE |  BUS_MASTER | MEMORY_SPACE | ~IO_SPACE
	device->pcie.write_config32(0x4, pcie_reg1);

	uint8_t pcap_ptr = device->pcie.read_config32(0x34) & 0xFC;
	uint8_t msix_ptr = 0;

	while(pcap_ptr)
	{
		auto capability = device->pcie.read_config32(pcap_ptr);

		if((capability & 0xFF) == 0x11)
		{
			msix_ptr = pcap_ptr;
			break;
		}

		pcap_ptr = (capability >> 8) & 0xFC; 
	}

	if(!msix_ptr)
	{
		log::error("nvme0: pcie device has no MSI-X capability");
		return;
	}

	auto msix_reg0 = device->pcie.read_config32(msix_ptr);
	uint16_t msix_mctr = msix_reg0 >> 16;
	auto msix_reg1 = device->pcie.read_config32(msix_ptr + 4);
	
	log::debug("nvme0: MSI-X using BIR {}, table at {:#x} size {:#x}", msix_reg1 & 0b111, (msix_reg1 & ~(0b111)), msix_mctr & 0b11111111111); 

	auto base = device->pcie.read_bar();
	//auto bar_size = device->pcie.get_bar_size();
	//FIXME: bar size detection broken
	size_t bar_size = 0x4000;
	log::debug("nvme0: BAR {:#x} size {:#x}", base, bar_size);

	device->base_address = base + mm::direct_mapping_offset;
	vm_map_range(base, device->base_address, bar_size, vm_write | vm_mmio);

	auto ctrl_cap = device->read_register<nvme_capability>(NVME_REGISTER_CAPABILITY);
	
	log::debug("nvme0: max entries: {}", ctrl_cap.max_queue_entries + 1);
	log::debug("nvme0: needs contiguous queue: {}", bool(ctrl_cap.contiguous_queue_required));
	log::debug("nvme0: timeout: {}", ctrl_cap.timeout);
	log::debug("nvme0: doorbell stride: {}", uint8_t(ctrl_cap.doorbell_stride));
	log::debug("nvme0: memory page size: {} - {}", uint8_t(ctrl_cap.memory_pagesize_min), uint8_t(ctrl_cap.memory_pagesize_max));

	auto stride = ctrl_cap.doorbell_stride;
	device->doorbell_stride = stride;

	auto config = device->read_register<nvme_config>(NVME_REGISTER_CONTROLLER_CONFIG);
	if(config.enabled)
	{
		config.enabled = 0;
		log::debug("nvme0: resetting controller");
		device->write_register<nvme_config>(NVME_REGISTER_CONTROLLER_CONFIG, config);
	}
		
	while(device->read_register<nvme_status>(NVME_REGISTER_CONTROLLER_STATUS).ready)
	{
		asm volatile("pause");
	}
	
	device->submission_queue.address = (void*)vmalloc(0x1000, vm_write);
	device->submission_queue.phys_address =	get_kernel_vmspace()->get_mapping((virtaddr_t)device->submission_queue.address); 
	device->submission_queue.size = 64;
	device->submission_queue.tail = 0;
	device->submission_queue.entry_size = 64; 
	device->write_register<physaddr_t>(NVME_REGISTER_ADMIN_SUBMISSION_QUEUE, device->submission_queue.phys_address);

	device->completion_queue.address = (void*)vmalloc(0x1000, vm_write);
	device->completion_queue.phys_address = get_kernel_vmspace()->get_mapping((virtaddr_t)device->completion_queue.address);
	device->completion_queue.size = 64;
	device->completion_queue.tail = 0;
	device->completion_queue.entry_size = 16;
	device->write_register<physaddr_t>(NVME_REGISTER_ADMIN_COMPLETION_QUEUE, device->completion_queue.phys_address);

	uint32_t aqs = (device->submission_queue.size - 1) & 0x7FF | (((device->completion_queue.size - 1) & 0x7FF) << 16);
	device->write_register<uint32_t>(NVME_REGISTER_ADMIN_QUEUE_ATTR, aqs);	
	
	physaddr_t msi_address = 0xfee00000;
	auto irq = allocate_irq();
       	install_irq_handler(irq, nvme_irq_handler);	

	log::debug("nvme0: allocated irq {}", irq);

	msix_reg0 &= ~(1 << 30); // clear FUNCTION_MASK
	msix_reg0 |= (1 << 31); // set ENABLE
	device->pcie.write_config32(msix_ptr, msix_reg0);
	
	auto msi_table_offset = msix_reg1 & ~(0b111);
	device->write_register<uint32_t>(msi_table_offset, msi_address & (~0x3));
 	device->write_register<uint32_t>(msi_table_offset + 4, msi_address >> 32);
	device->write_register<uint32_t>(msi_table_offset + 8, irq & 0xFF);
	device->write_register<uint32_t>(msi_table_offset + 12, 0);	

	log::debug("nvme0: enabled MSI-X interrupts");

	config.enabled = 1;
	config.io_commandset_selected = 0; // NVM_COMMAND_SET
	config.memory_pagesize = 0; // 4k
	config.arbitration_mechanism_selected = 0; // AMS_ROUNDROBIN
	config.shutdown_notification = 0;
	config.io_sq_entrysize = 6; // 64 bytes
	config.io_cq_entrysize = 4; // 16 bytes

	device->write_register<nvme_config>(NVME_REGISTER_CONTROLLER_CONFIG, config);

	while(true)
	{
		auto status = device->read_register<nvme_status>(NVME_REGISTER_CONTROLLER_STATUS);
		if(status.ready)
			break;

		if(status.fatal_status)
		{
			log::error("nvme0: controller failed to start");
			return;
		}

		asm volatile("pause");
	}

	log::debug("nvme0: controller started!");
	
	device->identify = (nvme_identify_controller*)vmalloc(0x1000, vm_write);

	nvme_cmd cmd{};
	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	cmd.nsid = 0;
	cmd.identify.cns = 1;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)device->identify);
	cmd.prp2 = 0;

	nvme_cmd* submission = (nvme_cmd*)device->submission_queue.allocate();
	nvme_completion* completion = (nvme_completion*)device->completion_queue.allocate();
	memcpy(submission, &cmd, sizeof(nvme_cmd));

	log::debug("nvme0: send identify cmd");
	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE, device->submission_queue.tail);

	while(!intr_wait)
	{
		asm volatile("pause");
	}
	intr_wait = false;

	while(true)
	{
		if(completion->phase_tag == 1)
			break;

		asm volatile("pause");
	}

	log::debug("nvme0: cmd completed with status {}", uint16_t(completion->status));
	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + 1 * (4 << stride), device->completion_queue.tail);

	size_t shift = 12 + ctrl_cap.memory_pagesize_min; 
	log::debug("nvme0: max data transfer size: {}", 1 << (device->identify->max_data_transfer_size + shift));
	log::debug("nvme0: {} namespaces", device->identify->num_namespaces);
	log::debug("nvme0: submission queue entry size: {}", 1 << device->identify->submit_queue_entry_size);
	log::debug("nvme0: completion queue entry size: {}", 1 << device->identify->completion_queue_entry_size);
	log::debug("nvme0: max outstanding commands: {}", device->identify->maxcmd + 1);

	if(device->identify->max_data_transfer_size)
		device->max_transfer = 1 << (device->identify->max_data_transfer_size + shift);
	else
		device->max_transfer = 1 << 20;

	auto num_namespaces = device->identify->num_namespaces;

	auto nsid_alloc = vmalloc(0x1000, vm_write);

	cmd.identify.cns = 2;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)nsid_alloc);
	submission = (nvme_cmd*)device->submission_queue.allocate();
	completion = (nvme_completion*)device->completion_queue.allocate();
	memcpy(submission, &cmd, sizeof(nvme_cmd));

	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE, device->submission_queue.tail);
	log::debug("nvme0: identify namespaces");

	while(true)
	{
		if(completion->phase_tag == 1)
			break;

		asm volatile("pause");
	}
	log::debug("nvme0: cmd completed with status {}", uint16_t(completion->status));

	device->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + 1 * (4 << stride), device->completion_queue.tail);

	auto* nsids = reinterpret_cast<uint32_t*>(nsid_alloc);
	for(uint32_t i = 0; i < num_namespaces; i++)
	{
		if(nsids[i] == 0)
			continue;

		nvme_register_namespace(device, nsids[i]);
	}

};

