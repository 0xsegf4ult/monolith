#include <dev/nvme/nvme.h>
#include <dev/nvme/nvme_structs.h>
#include <dev/pcie/pcie.h>
#include <dev/disk.h>
#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sys/device.h>
#include <libk/string.h>
#include <libk/vsprintf.h>
#include <errno.h>
#include <cpu.h>
#include <types.h>
#include <klog.h>

static uint32_t controller_count = 0;

struct nvme_queue
{
	void* address;
	physaddr_t phys_address;
	uint32_t size;
	uint32_t tail;
	uint32_t entry_size;
};

struct nvme_device
{
	struct pcie_device* pcie;
	virtaddr_t base_address;
	struct nvme_queue submission_queue;
	struct nvme_queue completion_queue;
	struct nvme_queue io_sq;
	struct nvme_queue io_cq;
	nvme_identify_controller* identify;

	uint16_t id;
	uint8_t doorbell_stride;
	uint8_t mps_shift;
};

struct nvme_namespace
{
	size_t nsid;
	struct nvme_device* parent;
	nvme_identify_namespace* identify;
	struct disk* disk;

	virtaddr_t dma_page;
	physaddr_t dma_page_phys;

	size_t block_size;
	size_t blocks;
};

static uint64_t nvme_read64(struct nvme_device* device, uint32_t offset)
{
	return *(const volatile uint64_t*)(device->base_address + offset);
}

static void nvme_write64(struct nvme_device* device, uint32_t offset, uint64_t data)
{
	*(volatile uint64_t*)(device->base_address + offset) = data;
}

static uint32_t nvme_read32(struct nvme_device* device, uint32_t offset)
{
	return *(const volatile uint32_t*)(device->base_address + offset);
}

static void nvme_write32(struct nvme_device* device, uint32_t offset, uint32_t data)
{
	*(volatile uint32_t*)(device->base_address + offset) = data;
}

static void* nvme_queue_allocate(struct nvme_queue* queue)
{
	void* res = (void*)((byte*)queue->address + queue->tail * queue->entry_size);
	queue->tail = (queue->tail + 1) % queue->size;
	return res;
}

static uint16_t nvme_submit_admin_await(struct nvme_device* device, nvme_cmd* cmd)
{
	nvme_cmd* submission = (nvme_cmd*)nvme_queue_allocate(&device->submission_queue);
	nvme_completion* completion = (nvme_completion*)nvme_queue_allocate(&device->completion_queue);
	memcpy(submission, cmd, sizeof(nvme_cmd));

	nvme_write32(device, NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE, device->submission_queue.tail);

	while(true)
	{
		if(completion->phase_tag == 1)
			break;

		native_cpu_relax();
	}

	uint16_t status = completion->status;
	nvme_write32(device, NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + 1 * (4 << device->doorbell_stride), device->completion_queue.tail);
	return status;
}

static void nvme_controller_reset(struct nvme_device* device)
{
	nvme_config cfg =
	{
		.raw = nvme_read32(device, NVME_REGISTER_CONTROLLER_CONFIG)
	};

	if(cfg.enabled)
	{
		cfg.enabled = 0;
		klog("nvme%d: resetting controller\n", device->id);
		nvme_write32(device, NVME_REGISTER_CONTROLLER_CONFIG, cfg.raw);
	}

	for(;;)
	{
		nvme_status status =
		{
			.raw = nvme_read32(device, NVME_REGISTER_CONTROLLER_STATUS)
		};

		if(!status.ready)
			break;

		native_cpu_relax();
	}
}

static void nvme_admin_queue_init(struct nvme_device* device)
{
	nvme_capability cap =
	{
		.raw = nvme_read64(device, NVME_REGISTER_CAPABILITY)
	};

	device->doorbell_stride = cap.doorbell_stride;	
	device->mps_shift = 12 + cap.memory_pagesize_min;

	device->submission_queue.address = vmalloc(0x1000);
	device->submission_queue.phys_address = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)device->submission_queue.address).base;
	device->submission_queue.size = 64;
	device->submission_queue.tail = 0;
	device->submission_queue.entry_size = 64;
	nvme_write64(device, NVME_REGISTER_ADMIN_SUBMISSION_QUEUE, device->submission_queue.phys_address);
	
	device->completion_queue.address = vmalloc(0x1000);
	device->completion_queue.phys_address = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)device->completion_queue.address).base;
	device->completion_queue.size = 64;
	device->completion_queue.tail = 0;
	device->completion_queue.entry_size = 16;
	nvme_write64(device, NVME_REGISTER_ADMIN_COMPLETION_QUEUE, device->completion_queue.phys_address);

	uint32_t aqs = (device->submission_queue.size - 1) & 0x7FF | (((device->completion_queue.size - 1) & 0x7FF) << 16);
	nvme_write32(device, NVME_REGISTER_ADMIN_QUEUE_ATTR, aqs);
}

static void nvme_controller_start(struct nvme_device* device)
{
	nvme_config cfg =
	{
		.raw = nvme_read32(device, NVME_REGISTER_CONTROLLER_CONFIG)
	};

	cfg.enabled = 1;
	cfg.io_commandset_selected = 0; // NVM_COMMAND_SET
	cfg.memory_pagesize = 0; // 4K
	cfg.arbitration_mechanism_selected = 0; // AMS_ROUNDROBIN
	cfg.shutdown_notification = 0;
	cfg.io_sq_entrysize = 6; // 64
	cfg.io_cq_entrysize = 4; // 16
	
	nvme_write32(device, NVME_REGISTER_CONTROLLER_CONFIG, cfg.raw);

	for(;;)
	{
		nvme_status status =
		{
			.raw = nvme_read32(device, NVME_REGISTER_CONTROLLER_STATUS)
		};

		if(status.ready)
			break;

		if(status.fatal_status)
		{
			klog("nvme%d: controller failed to start\n", device->id);
			return;
		}

		native_cpu_relax();
	}

	klog("nvme%d: controller started!\n", device->id);
}

static void nvme_controller_identify(struct nvme_device* device)
{
	device->identify = (nvme_identify_controller*)vmalloc(0x1000);

	nvme_cmd cmd = {};
	cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	cmd.nsid = 0;
	cmd.identify.cns = 1;
	cmd.prp1 = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)device->identify).base;
	cmd.prp2 = 0;

	nvme_submit_admin_await(device, &cmd);
}

static void nvme_io_queue_init(struct nvme_device* device)
{
	device->io_cq.address = vmalloc(0x1000);
	device->io_cq.phys_address = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)device->io_cq.address).base;
	device->io_cq.size = 64;
	device->io_cq.tail = 0;
	device->io_cq.entry_size = 16;

	nvme_cmd cq_cmd = {};
	cq_cmd.header.opcode = NVME_ADMIN_OP_CREATE_CQ;
	cq_cmd.prp1 = device->io_cq.phys_address;
	cq_cmd.create_io_cq.qid = 1;
	cq_cmd.create_io_cq.qsize = 63;
	cq_cmd.create_io_cq.iv = 0;
	cq_cmd.create_io_cq.phys_contig = true;
	cq_cmd.create_io_cq.interrupt_enable = true;
	nvme_submit_admin_await(device, &cq_cmd);

	device->io_sq.address = vmalloc(0x1000);
	device->io_sq.phys_address = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)device->io_sq.address).base;
	device->io_sq.size = 64;
	device->io_sq.tail = 0;
	device->io_sq.entry_size = 64;

	nvme_cmd sq_cmd = {};
	sq_cmd.header.opcode = NVME_ADMIN_OP_CREATE_SQ;
	sq_cmd.prp1 = device->io_sq.phys_address;
	sq_cmd.create_io_sq.qid = 1;
	sq_cmd.create_io_sq.qsize = 63;
	sq_cmd.create_io_sq.cqid = 1;
	sq_cmd.create_io_sq.phys_contig = true;
	sq_cmd.create_io_sq.qprio = 0;
	sq_cmd.create_io_sq.nvmsetid = 0;
	nvme_submit_admin_await(device, &sq_cmd);
}

ssize_t nvme_read(struct nvme_namespace* ns, uint64_t lba, size_t blocks, byte* buffer)
{
	size_t offset = 0;
	while(offset < blocks)
	{
		uint16_t count = blocks - offset;
		if(count > (0x1000 / ns->block_size))
			count = (0x1000 / ns->block_size);

		nvme_cmd* submission = (nvme_cmd*)nvme_queue_allocate(&ns->parent->io_sq);
		memset(submission, 0, sizeof(nvme_cmd));
		submission->header.opcode = NVME_OP_READ;
		submission->nsid = ns->nsid;
		submission->prp1 = ns->dma_page_phys;
		submission->rw.lba = lba + offset;
		submission->rw.nlb = count - 1;
		
		nvme_completion* completion = (nvme_completion*)nvme_queue_allocate(&ns->parent->io_cq);
		nvme_write32(ns->parent, NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + (2 * (4 << ns->parent->doorbell_stride)), ns->parent->io_sq.tail);

		while(true)
		{
			if(completion->phase_tag == 1)
				break;

			native_cpu_relax();
		}

		memcpy(buffer + (offset * ns->block_size), (void*)ns->dma_page, count * ns->block_size);
		nvme_write32(ns->parent, NVME_REGISTER_QUEUE_TAIL_DOORBELL_BASE + (3 * (4 << ns->parent->doorbell_stride)), ns->parent->io_cq.tail);
		offset += count;
	}

	return blocks;
}

ssize_t nvme_file_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	struct disk* disk = (struct disk*)blockdev_lookup(file->inode->dev)->data;
	struct nvme_namespace* ns = (struct nvme_namespace*)disk->data;
	if(!ns)
		return -ENODEV;

	size_t lba = file->pos / ns->block_size;
	size_t blocks = length / ns->block_size;

	return nvme_read(ns, lba, blocks, buffer) * ns->block_size;
}

ssize_t nvme_file_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	return -ENOTSUP;
}

int nvme_ioctl(struct file_descriptor* file, uint64_t op, uint64_t arg)
{
	return -EINVAL;
}

static struct file_ops nvme_ns_fops =
{
	.read = nvme_file_read,
	.write = nvme_file_write,
	.ioctl = nvme_ioctl
};

ssize_t nvme_read_blocks(struct block_device* dev, byte* buffer, size_t blocks, off_t b_offset)
{
	struct disk* disk = (struct disk*)dev->data;
	return nvme_read((struct nvme_namespace*)disk->data, b_offset, blocks, buffer);
}

static struct blockdev_ops nvme_ns_bops =
{
	.pread_blocks = nvme_read_blocks
};

static void nvme_namespace_register(struct nvme_device* device, uint32_t ns)
{
	struct nvme_namespace* nvme_ns = kmalloc(sizeof(struct nvme_namespace));
	nvme_ns->nsid = ns;
	nvme_ns->parent = device;

	char devname[32];
	sprintf(devname, "nvme%dn%d", device->id, ns);

	nvme_ns->identify = (nvme_identify_namespace*)vmalloc(0x1000);

	nvme_cmd id_cmd = {};
	id_cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	id_cmd.nsid = ns;
	id_cmd.identify.cns = 0;
	id_cmd.prp1 = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)nvme_ns->identify).base;
	id_cmd.prp2 = 0;
	nvme_submit_admin_await(device, &id_cmd);

	uint64_t flba = nvme_ns->identify->flbas & 0xF;
	uint64_t lba_shift = nvme_ns->identify->lbaf[flba].lba_data_size;

	nvme_ns->dma_page = (virtaddr_t)vmalloc(0x1000);
	nvme_ns->dma_page_phys = vm_space_get_mapping(vm_get_kernel_space(), nvme_ns->dma_page).base;
	nvme_ns->disk = disk_create(make_dev(4, ((ns - 1) * 128)), devname, nvme_ns, &nvme_ns_fops, &nvme_ns_bops);
	nvme_ns->block_size = (1 << lba_shift);
	nvme_ns->blocks = nvme_ns->identify->nsze;
	nvme_ns->disk->block_count = nvme_ns->blocks;
	nvme_ns->disk->block_size = nvme_ns->block_size;
	disk_scan(nvme_ns->disk);
}

static void nvme_namespaces_identify(struct nvme_device* device)
{
	uint32_t* nsid_alloc = vmalloc(0x1000);

	nvme_cmd id_cmd = {};
	id_cmd.header.opcode = NVME_ADMIN_OP_IDENTIFY;
	id_cmd.nsid = 0;
	id_cmd.identify.cns = 2;
	id_cmd.prp1 = vm_space_get_mapping(vm_get_kernel_space(), (virtaddr_t)nsid_alloc).base;
	id_cmd.prp2 = 0;

	nvme_submit_admin_await(device, &id_cmd);

	for(uint32_t i = 0; i < device->identify->num_namespaces; i++)
	{
		if(nsid_alloc[i] == 0)
			continue;

		nvme_namespace_register(device, nsid_alloc[i]);
	}

	vfree(nsid_alloc);
}

void nvme_controller_init(struct pcie_device* pdev)
{
	uint32_t cid = controller_count++;
	dev_t id = make_dev(4, cid);

	struct char_device* cdev = chardev_new(id);
	char name_buf[32];
	sprintf(name_buf, "/dev/nvme%d", cid);
	vfs_mknod(name_buf, S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, id);

	struct nvme_device* device = kmalloc(sizeof(struct nvme_device));
	device->pcie = pdev;
	device->id = cid;
	cdev->data = device;

	uint32_t pcie_reg1 = pcie_read32(pdev, 0x4);
	pcie_reg1 |= ((0x400 | 0x4 | 0x2) & ~(0x1)); // INTERRUPT_DISABLE | BUS_MASTER | MEMORY_SPACE | ~IO_SPACE
	pcie_write32(pdev, 0x4, pcie_reg1);

	struct pcie_bar bar0 = pcie_get_bar(pdev, 0);
	device->base_address = bar0.address;

	if(!pcie_enable_msix(pdev))
	{
		klog("nvme%d: PCIe device has no MSI-X capability\n", cid);
		return;
	}

	nvme_controller_reset(device);
	nvme_admin_queue_init(device);
	nvme_controller_start(device);
	nvme_controller_identify(device);
	nvme_io_queue_init(device);
	nvme_namespaces_identify(device);
}
