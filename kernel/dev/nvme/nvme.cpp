#include <dev/nvme/nvme.hpp>
#include <dev/nvme/nvme_structs.hpp>

#include <arch/x86_64/interrupts.hpp>

#include <dev/pcie.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>
#include <dev/block/block.hpp>
#include <dev/block/disk.hpp>

#include <fs/ops.hpp>
#include <fs/vfs.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <kstd.hpp>
#include <klog.hpp>

#include <sys/err.hpp>

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
	nvme_queue io_sq;
	nvme_queue io_cq;
	nvme_identify_controller* identify;

	uint16_t id;
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
		*reinterpret_cast<T*>(base_address + offset) = value;
	}

	void admin_submit_await(nvme_cmd& cmd)
	{
		auto* submission = (nvme_cmd*)submission_queue.allocate();
		auto* completion = (nvme_completion*)completion_queue.allocate();
		memcpy(submission, &cmd, sizeof(nvme_cmd));
	
		write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE, submission_queue.tail);

		while(true)
		{
			if(completion->phase_tag == 1)
				break;

			asm volatile("pause");
		}
		log::debug("nvme{}: cmd completed with status {}", id, uint16_t(completion->status));

		write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + 1 * (4 << doorbell_stride), completion_queue.tail);
	}
};

struct nvme_namespace
{
	size_t nsid;
	nvme_device* parent;
	nvme_identify_namespace* identify;
	disk_t* disk;

	virtaddr_t dma_page;
	physaddr_t dma_page_phys;

	size_t block_size;
	size_t blocks;
};

static bool intr_wait = false;
void nvme_irq_handler()
{
	log::debug("nvme0: interrupt");
	intr_wait = true;
}

ssize_t nvme_read(nvme_namespace* ns, uint64_t lba, size_t blocks, byte* buffer)
{
	size_t offset = 0;
	while(offset < blocks)
	{
		uint16_t count = blocks - offset;
	       	if(count > (0x1000 / ns->block_size))
			count = (0x1000 / ns->block_size);

		nvme_cmd cmd{};
		cmd.header.opcode = NVME_OP_READ;
		cmd.nsid = ns->nsid;
		cmd.prp1 = ns->dma_page_phys;
		cmd.rw.lba = lba + offset;
		cmd.rw.nlb = count - 1;
		auto* submission = (nvme_cmd*)ns->parent->io_sq.allocate();
		auto* completion = (nvme_completion*)ns->parent->io_cq.allocate();
		memcpy(submission, &cmd, sizeof(nvme_cmd));
		ns->parent->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + (2 * (4 << ns->parent->doorbell_stride)), ns->parent->io_sq.tail);

		while(true)
		{
			if(completion->phase_tag == 1)
				break;

			asm volatile("pause");
		}

		memcpy(buffer + (offset * ns->block_size), (void*)ns->dma_page, count * ns->block_size);
		
		ns->parent->write_register<uint32_t>(NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + (3 * (4 << ns->parent->doorbell_stride)), ns->parent->io_cq.tail);
		offset += count;
	}	

	return blocks;
}

ssize_t nvme_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* disk = (disk_t*)(blockdev_get(file->inode->dev)->data);
	auto* ns = (nvme_namespace*)(disk->data);
	if(!ns)
		return -ENOENT;

	auto lba = file->read_pos / ns->block_size;
       	auto blocks = length / ns->block_size;	

	return nvme_read(ns, lba, blocks, buffer) * ns->block_size;
}

ssize_t nvme_write(vfs::file_descriptor_t* file, const byte* buffer, size_t length)
{
	return 0;
}

int nvme_ioctl(vfs::file_descriptor_t* file, uint64_t op, uint64_t arg)
{
	return -EINVAL;
}

static vfs::fs_file_ops nvme_ns_fops =
{
	.read = nvme_read,
	.write = nvme_write,
	.ioctl = nvme_ioctl
};

ssize_t nvme_read_blocks(block_device_t* dev, byte* buffer, size_t blocks, size_t b_offset)
{
	auto* disk = (disk_t*)dev->data;
	return nvme_read((nvme_namespace*)disk->data, b_offset, blocks, buffer);
}

static blockdev_ops nvme_ns_bops =
{
	.pread_blocks = nvme_read_blocks
};

void nvme_register_namespace(nvme_device* device, uint32_t ns)
{
	auto* nvme_ns = (nvme_namespace*)kmalloc(sizeof(nvme_namespace));
	nvme_ns->nsid = ns;
	nvme_ns->parent = device;
	
	char devname[32];
	format_to(string_span{&devname[0], 32}, "nvme{}n{}", device->id, ns);

	nvme_ns->identify = (nvme_identify_namespace*)vmalloc(0x1000, vm_write);

	nvme_cmd cmd{};
	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	cmd.nsid = ns;
	cmd.identify.cns = 0;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)nvme_ns->identify);
	cmd.prp2 = 0;
	
	log::debug("{}: send identify command", devname);
	device->admin_submit_await(cmd);

	uint64_t flba = nvme_ns->identify->flbas & 0xF;
	uint64_t lba_shift = nvme_ns->identify->lbaf[flba].lba_data_size;
	uint64_t max_lba = (device->max_transfer) / (1 << lba_shift);
	
	log::debug("{}: {:#x} blocks blk_size {:#x} -> {} MiB", devname, nvme_ns->identify->nsze, 1 << lba_shift, nvme_ns->identify->nsze * (1 << lba_shift) / (1 << 20));

	nvme_ns->dma_page = vmalloc(0x1000, vm_write);
	nvme_ns->dma_page_phys = get_kernel_vmspace()->get_mapping(nvme_ns->dma_page);

	nvme_ns->disk = disk_create(dev_t{4, uint16_t((ns - 1) * 128)}, devname, nvme_ns, &nvme_ns_fops, &nvme_ns_bops);
	nvme_ns->block_size = (1 << lba_shift);
	nvme_ns->blocks = nvme_ns->identify->nsze;
	nvme_ns->disk->block_count = nvme_ns->blocks;
       	nvme_ns->disk->block_size = nvme_ns->block_size;
	disk_scan(nvme_ns->disk);	
}

static uint16_t controller_count = 0;

void nvme_init_controller(pcie_device& dev)
{
	auto id = controller_count++;

	auto* chdev = chardev_alloc(dev_t{4, id});

	auto* device = (nvme_device*)kmalloc(sizeof(nvme_device));
	device->pcie = dev;
	device->id = id;
	chdev->data = device;

	char name_buf[32];
	format_to(string_span{&name_buf[0], 32}, "/dev/nvme{}", id);
	auto dev_node = vfs::mknod(name_buf, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dev_t{4, id});

	auto pcie_reg1 = device->pcie.read_config32(0x4);
	pcie_reg1 |= ((0x400 | 0x4 | 0x2) & ~(0x1)); // INTERRUPT_DISABLE |  BUS_MASTER | MEMORY_SPACE | ~IO_SPACE
	device->pcie.write_config32(0x4, pcie_reg1);

	msix_descriptor_t msix;
	auto msix_status = device->pcie.enable_msix(msix);
	if(!msix_status)
	{
		log::error("nvme{}: PCIe device has no MSI-X capability", id);
		return;
	}

	auto base = device->pcie.read_bar(0);
	auto bar_size = device->pcie.get_bar_size(0);

	device->base_address = vmalloc(bar_size, vm_write | vm_mmio, &base);

	auto ctrl_cap = device->read_register<nvme_capability>(NVME_REGISTER_CAPABILITY);
	
	auto stride = ctrl_cap.doorbell_stride;
	device->doorbell_stride = stride;

	auto config = device->read_register<nvme_config>(NVME_REGISTER_CONTROLLER_CONFIG);
	if(config.enabled)
	{
		config.enabled = 0;
		log::debug("nvme{}: resetting controller", id);
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

	log::debug("nvme{}: allocated irq {}", id, irq);

	auto* msix_table = msix.table;
	*(msix_table++) = (msi_address & ~0x3);
	*(msix_table++) = (msi_address >> 32);
        *(msix_table++) = (irq & 0xFF);
	*(msix_table++) = 0;	

	log::debug("nvme{}: enabled MSI-X interrupts", id);

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
			log::error("nvme{}: controller failed to start", id);
			return;
		}

		asm volatile("pause");
	}

	log::debug("nvme{}: controller started!", id);
	
	device->identify = (nvme_identify_controller*)vmalloc(0x1000, vm_write);

	nvme_cmd cmd{};
	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	cmd.nsid = 0;
	cmd.identify.cns = 1;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)device->identify);
	cmd.prp2 = 0;

	log::debug("nvme{}: send identify cmd", id);
	device->admin_submit_await(cmd);

	size_t shift = 12 + ctrl_cap.memory_pagesize_min; 
	if(device->identify->max_data_transfer_size)
		device->max_transfer = 1 << (device->identify->max_data_transfer_size + shift);
	else
		device->max_transfer = 1 << 20;

	auto num_namespaces = device->identify->num_namespaces;

	device->io_cq.address = (void*)vmalloc(0x1000, vm_write);
	device->io_cq.phys_address = get_kernel_vmspace()->get_mapping((virtaddr_t)device->io_cq.address);
	device->io_cq.size = 64;
	device->io_cq.tail = 0;
	device->io_cq.entry_size = 16;

	nvme_cmd cq_cmd{};
	cq_cmd.header.opcode = NVME_ADMIN_OP_CREATE_CQ;
	cq_cmd.prp1 = device->io_cq.phys_address;
	cq_cmd.create_io_cq.qid = 1;
	cq_cmd.create_io_cq.qsize = 63;
	cq_cmd.create_io_cq.iv = 0;
	cq_cmd.create_io_cq.phys_contig = true;
	cq_cmd.create_io_cq.interrupt_enable = true;
	
	log::debug("nvme{}: cmd_create_cq", id);
	device->admin_submit_await(cq_cmd);

	device->io_sq.address = (void*)vmalloc(0x1000, vm_write);
	device->io_sq.phys_address = get_kernel_vmspace()->get_mapping((virtaddr_t)device->io_sq.address);
	device->io_sq.size = 64;
	device->io_sq.tail = 0;
	device->io_sq.entry_size = 64;

	nvme_cmd sq_cmd{};
	sq_cmd.header.opcode = NVME_ADMIN_OP_CREATE_SQ;
	sq_cmd.prp1 = device->io_sq.phys_address;
	sq_cmd.create_io_sq.qid = 1;
	sq_cmd.create_io_sq.qsize = 63;
	sq_cmd.create_io_sq.cqid = 1;
	sq_cmd.create_io_sq.phys_contig = true;
	sq_cmd.create_io_sq.qprio = 0;
	sq_cmd.create_io_sq.nvmsetid = 0;
	
	log::debug("nvme{}: cmd_create_sq", id);
	device->admin_submit_await(sq_cmd);

	auto nsid_alloc = vmalloc(0x1000, vm_write);

	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY; 
	cmd.nsid = 0;
	cmd.identify.cns = 2;
	cmd.prp1 = get_kernel_vmspace()->get_mapping((virtaddr_t)nsid_alloc);
	cmd.prp2 = 0;
	
	log::debug("nvme{}: identify namespaces", id);
	device->admin_submit_await(cmd);

	auto* nsids = reinterpret_cast<uint32_t*>(nsid_alloc);
	for(uint32_t i = 0; i < num_namespaces; i++)
	{
		if(nsids[i] == 0)
			continue;

		nvme_register_namespace(device, nsids[i]);
	}

};

