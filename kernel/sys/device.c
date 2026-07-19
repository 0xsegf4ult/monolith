#include <sys/device.h>
#include <fs/ops.h>
#include <mm/slab.h>
#include <types.h>

static struct char_device* char_devices[256] = {0};
static spinlock_t chardev_lock = {0};

static struct block_device* block_devices[256] = {0};
static spinlock_t blockdev_lock = {0};

static struct file_ops default_fops = {};
static struct blockdev_ops default_bd_ops = {};

struct char_device* chardev_new(dev_t dev)
{
	uint32_t hash = dev_major(dev) % 256;

	struct char_device* device = kmalloc(sizeof(struct char_device));
	device->dev = dev;
       	device->data = nullptr;
	device->fops = &default_fops;

	uint64_t flags;
	spinlock_acquire_irqsave(&chardev_lock, &flags);
	device->next = char_devices[hash];
	char_devices[hash] = device;
	spinlock_release_irqsave(&chardev_lock, flags);

	return device;	
}

struct char_device* chardev_lookup(dev_t dev)
{
	uint32_t hash = dev_major(dev) % 256;

	uint64_t flags;
	spinlock_acquire_irqsave(&chardev_lock, &flags);
	struct char_device* cur = char_devices[hash];
	while(cur)
	{
		if(cur->dev == dev)
		{
			spinlock_release_irqsave(&chardev_lock, flags);
			return cur;
		}

		cur = cur->next;
	}
	spinlock_release_irqsave(&chardev_lock, flags);

	return nullptr;
}

struct block_device* blockdev_new(dev_t dev)
{
	uint32_t hash = dev_major(dev) % 256;

	struct block_device* device = kmalloc(sizeof(struct block_device));
	device->dev = dev;
	device->data = nullptr;
	device->fops = &default_fops;
	device->bd_ops = &default_bd_ops;
	
	uint64_t flags;
	spinlock_acquire_irqsave(&blockdev_lock, &flags);
	device->next = block_devices[hash];
	block_devices[hash] = device;
	spinlock_release_irqsave(&blockdev_lock, flags);

	return device;
}

struct block_device* blockdev_lookup(dev_t dev)
{
	uint32_t hash = dev_major(dev) % 256;

	uint64_t flags;
	spinlock_acquire_irqsave(&blockdev_lock, &flags);
	struct block_device* cur = block_devices[hash];
	while(cur)
	{
		if(cur->dev == dev)
		{
			spinlock_release_irqsave(&blockdev_lock, flags);
			return cur;
		}

		cur = cur->next;
	}
	spinlock_release_irqsave(&blockdev_lock, flags);

	return nullptr;
}
