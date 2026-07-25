#include <dev/net/e1000/e1000.h>
#include <dev/net/e1000/e1000_regs.h>
#include <dev/pcie/pcie.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <net/netdev.h>
#include <net/ether.h>
#include <sys/irq.h>
#include <libk/string.h>
#include <libk/vsprintf.h>
#include <klog.h>
#include <types.h>

constexpr size_t E1000_RX_DESC_COUNT = 256;
constexpr size_t E1000_TX_DESC_COUNT = 256;

typedef struct __attribute__((packed))
{
	physaddr_t addr;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} e1000_rx_desc_t;

typedef struct __attribute__((packed))
{
	physaddr_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
} e1000_tx_desc_t;

struct e1000_device
{
	struct pcie_device* pcie;
	struct netdev* netdev;

	virtaddr_t base_address;
	e1000_rx_desc_t* rx_descs;
	virtaddr_t rx_page[E1000_RX_DESC_COUNT];
	e1000_tx_desc_t* tx_descs;
	virtaddr_t tx_page[E1000_TX_DESC_COUNT];
	uint32_t cur_rx;
	uint32_t cur_tx;

	uint32_t link;
	bool has_eeprom;
};

static void cmd_write32(struct e1000_device* dev, uint16_t addr, uint32_t value)
{
	*(volatile uint32_t*)(dev->base_address + addr) = value;
}

static uint32_t cmd_read32(struct e1000_device* dev, uint16_t addr)
{
	return *(const volatile uint32_t*)(dev->base_address + addr);
}

static void e1000_read_hwaddr(struct e1000_device* device)
{
	uint32_t ral = cmd_read32(device, E1000_REG_RXADDR);
	uint32_t rah = cmd_read32(device, E1000_REG_RXADDR_HIGH);

	device->netdev->mac_addr[0] = ral & 0xFF;
	device->netdev->mac_addr[1] = (ral >> 8) & 0xFF;
	device->netdev->mac_addr[2] = (ral >> 16) & 0xFF;
	device->netdev->mac_addr[3] = (ral >> 24) & 0xFF;
	device->netdev->mac_addr[4] = rah & 0xFF;
	device->netdev->mac_addr[5] = (rah >> 8) & 0xFF;
}

static void e1000_set_link_up(struct e1000_device* device)
{
	uint32_t ctrl = cmd_read32(device, E1000_REG_CTRL);
	ctrl |= E1000_CTRL_SLU | E1000_CTRL_SPD_1000;
	ctrl &= ~(E1000_CTRL_LRST | E1000_CTRL_PHY_RST);
	cmd_write32(device, E1000_REG_CTRL, ctrl);

	device->link = (cmd_read32(device, E1000_REG_STATUS) & E1000_STATUS_LU);
}

static void e1000_init_rx(struct e1000_device* device)
{
	struct vm_space* kernel_vm = vm_get_kernel_space();
	device->rx_descs = (e1000_rx_desc_t*)vm_space_map(kernel_vm,
	(vm_mapping_info)
	{
		.length = sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT,
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_ALLOCATE
	});
	memset(device->rx_descs, 0, sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);

	for(int i = 0; i < E1000_RX_DESC_COUNT; i++)
	{
		device->rx_page[i] = vm_space_map(kernel_vm,
		(vm_mapping_info)
		{
			.length = 0x1000,
			.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
			.flags = VM_FLAG_ALLOCATE
		});
		memset((void*)device->rx_page[i], 0, 0x1000);
		device->rx_descs[i].addr = vm_space_get_mapping(kernel_vm, device->rx_page[i]).base;
		device->rx_descs[i].status = 0;
	}
	device->cur_rx = 0;

	physaddr_t rxd_phys = vm_space_get_mapping(kernel_vm, (virtaddr_t)device->rx_descs).base;
	cmd_write32(device, E1000_REG_RXDESC_HIGH, (uint32_t)(rxd_phys >> 32));
	cmd_write32(device, E1000_REG_RXDESC_LOW, (uint32_t)(rxd_phys & 0xFFFFFFFF));

	cmd_write32(device, E1000_REG_RXDESC_LEN, E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t));
	cmd_write32(device, E1000_REG_RXDESC_HEAD, 0);
	cmd_write32(device, E1000_REG_RXDESC_TAIL, E1000_RX_DESC_COUNT - 1);

	cmd_write32(device, E1000_REG_RCTRL,
		E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE |
		E1000_RCTL_MPE | E1000_RCTL_BAM | E1000_RCTL_SZ_4096 |
		E1000_RCTL_SECRC | E1000_RCTL_BSEX);
}

static void e1000_init_tx(struct e1000_device* device)
{
	struct vm_space* kernel_vm = vm_get_kernel_space();
	device->tx_descs = (e1000_tx_desc_t*)vm_space_map(kernel_vm,
	(vm_mapping_info)
	{
		.length = sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT,
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_ALLOCATE
	});
	memset(device->tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);

	for(int i = 0; i < E1000_TX_DESC_COUNT; i++)
	{
		device->tx_page[i] = vm_space_map(kernel_vm,
		(vm_mapping_info)
		{
			.length = 0x1000,
			.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
			.flags = VM_FLAG_ALLOCATE
		});
		memset((void*)device->tx_page[i], 0, 0x1000);
		device->tx_descs[i].addr = vm_space_get_mapping(kernel_vm, device->tx_page[i]).base;
		device->tx_descs[i].status = 0;
		device->tx_descs[i].cmd = E1000_CMD_EOP;
	}
	device->cur_tx = 0;

	physaddr_t txd_phys = vm_space_get_mapping(kernel_vm, (virtaddr_t)device->tx_descs).base;
	cmd_write32(device, E1000_REG_TXDESC_HIGH, (uint32_t)(txd_phys >> 32));
	cmd_write32(device, E1000_REG_TXDESC_LOW, (uint32_t)(txd_phys & 0xFFFFFFFF));
	
	cmd_write32(device, E1000_REG_TXDESC_LEN, E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t));
	cmd_write32(device, E1000_REG_TXDESC_HEAD, 0);
	cmd_write32(device, E1000_REG_TXDESC_TAIL, 0);

	uint32_t tctl = cmd_read32(device, E1000_REG_TCTRL);

	tctl &= (~0xFF << E1000_TCTL_CT_SHIFT);
	tctl |= (15 << E1000_TCTL_CT_SHIFT);
	tctl |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_RTLC;
	cmd_write32(device, E1000_REG_TCTRL, tctl);
}

static void e1000_irq(void* payload)
{
	struct e1000_device* device = (struct e1000_device*)payload;
	uint32_t icr = cmd_read32(device, E1000_REG_ICR);
	if(!icr)
		return;

	if(icr & E1000_ICR_RXQ0)
	{
		bool got_packet = false;
		uint32_t old_rx = 0;
		while(device->rx_descs[device->cur_rx].status & 0x1)
		{
			e1000_rx_desc_t* desc = device->rx_descs + device->cur_rx;
			desc->status = 0;

			struct ether_packet* eth = (struct ether_packet*)device->rx_page[device->cur_rx];
			ether_rx_packet(device->netdev, eth, desc->length);

			got_packet = true;

			old_rx = device->cur_rx;
			device->cur_rx = (device->cur_rx + 1) % E1000_RX_DESC_COUNT;
		}

		if(got_packet)
			cmd_write32(device, E1000_REG_RXDESC_TAIL, old_rx);
	}

	cmd_write32(device, E1000_REG_ICR, icr);
}

static ssize_t e1000_send(struct netdev* netdev, byte* buffer, size_t size)
{
	struct e1000_device* dev = (struct e1000_device*)netdev->data;

	memcpy((byte*)dev->tx_page[dev->cur_tx], buffer, size);


	e1000_tx_desc_t* desc = dev->tx_descs + dev->cur_tx;
	desc->length = size;
	desc->cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS | E1000_CMD_RPS;
	desc->status = 0;
	desc->css = 0;
	desc->special = 0;

	dev->cur_tx = (dev->cur_tx + 1) % E1000_TX_DESC_COUNT;
	cmd_write32(dev, E1000_REG_TXDESC_TAIL, dev->cur_tx);

	uint32_t status = cmd_read32(dev, E1000_REG_STATUS);

	return size;
}

static struct netdev_ops e1000_ops =
{
	.send = e1000_send
};

void e1000_init(struct pcie_device* pdev)
{
	struct e1000_device* device = vmalloc(sizeof(struct e1000_device));
       	device->pcie = pdev;
	device->netdev = netdev_create();
	device->netdev->ops = &e1000_ops;
	device->netdev->data = device;	
	sprintf(device->netdev->name, "enp%us%u", pdev->bus, pdev->device);

	uint32_t pcie_reg1 = pcie_read32(pdev, 0x4);
	pcie_reg1 |= ((0x400 | 0x4 | 0x2) & ~(0x1)); // INTERRUPT_DISABLE | BUS_MASTER | MEMORY_SPACE | ~IO_SPACE
	pcie_write32(pdev, 0x4, pcie_reg1);

	struct pcie_bar bar0 = pcie_get_bar(pdev, 0);
	klog("e1000: BAR0 %p size %x\n", bar0.address, bar0.size);
	device->base_address = bar0.address;

	if(!pcie_enable_msix(pdev))
	{
		klog("e1000: PCIe device has no MSI-X capability\n");
		return;
	}

	e1000_read_hwaddr(device);
	klog("e1000: netdevice %s HWADDR %x:%x:%x:%x:%x:%x\n", device->netdev->name, device->netdev->mac_addr[0], device->netdev->mac_addr[1], device->netdev->mac_addr[2], device->netdev->mac_addr[3], device->netdev->mac_addr[4], device->netdev->mac_addr[5]);

	e1000_set_link_up(device);
	klog("e1000: link is %s\n", device->link ? "UP" : "DOWN");

	e1000_init_rx(device);
	e1000_init_tx(device);

	irq_register_msi(pdev->msi_domain, 0, e1000_irq, device);
	cmd_write32(device, E1000_REG_IVAR, E1000_IVAR_ENTRY_VALID | (E1000_IVAR_ENTRY_VALID << 8) | (E1000_IVAR_ENTRY_VALID << 16));
	cmd_write32(device, E1000_REG_IMS, 
		E1000_ICR_TXDW | E1000_ICR_TXQE |
		E1000_ICR_LSC |
		E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_RXT0 | E1000_ICR_RXQ0 |
		E1000_ICR_SRPD |
		E1000_ICR_ACK);
}
