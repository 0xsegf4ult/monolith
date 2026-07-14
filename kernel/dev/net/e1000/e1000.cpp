#include <dev/net/e1000/e1000.hpp>
#include <dev/net/e1000/e1000_regs.hpp>
#include <dev/pcie.hpp>
#include <arch/x86_64/interrupts.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <net/netdev.hpp>
#include <net/ether.hpp>

// One page worth of descriptors, we cannot guarantee physical addresses will be contiguous from vmalloc
constexpr size_t E1000_RX_DESC_COUNT = 256;
constexpr size_t E1000_TX_DESC_COUNT = 256;

struct __attribute__((packed)) e1000_rx_desc_t
{
	physaddr_t addr;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
};

struct __attribute__((packed)) e1000_tx_desc_t
{
	physaddr_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

struct e1000_device
{
	pcie_device pcie;
	netdev_t* netdev;

	virtaddr_t base_address;
	virtaddr_t msi_table;
	e1000_rx_desc_t* rx_descs;
	virtaddr_t rx_page[E1000_RX_DESC_COUNT];
	e1000_tx_desc_t* tx_descs;
	virtaddr_t tx_page[E1000_TX_DESC_COUNT];
	uint32_t cur_rx;
	uint32_t cur_tx;

	uint32_t link;
	bool has_eeprom;

	void cmd_write32(uint16_t addr, uint32_t value)
	{
		*reinterpret_cast<volatile uint32_t*>(base_address + addr) = value;
	}

	uint32_t cmd_read32(uint16_t addr)
	{
		return *reinterpret_cast<const volatile uint32_t*>(base_address + addr);
	}

	uint16_t eeprom_read(uint8_t addr)
	{
		uint32_t result = 0;
		uint32_t eeprom_addr = (addr << 8) | 1;
		cmd_write32(E1000_REG_EEPROM, eeprom_addr);
		while(result = cmd_read32(E1000_REG_EEPROM), !(result & (1 << 4))) ;
		return (result >> 16);
	}
};

void e1000_detect_eeprom(e1000_device* device)
{
	device->cmd_write32(E1000_REG_EEPROM, 0x1);

	for(int i = 0; i < 1000; i++)
	{
		auto result = device->cmd_read32(E1000_REG_EEPROM);
		if(result & 0x10)
		{
			device->has_eeprom = true;
			return;
		}
	}
}

void e1000_read_mac(e1000_device* device)
{
	if(device->has_eeprom)
	{
		log::debug("e1000: has eeprom");
		uint16_t tmp;
		tmp = device->eeprom_read(0);
		device->netdev->mac_addr[0] = tmp & 0xFF;
		device->netdev->mac_addr[1] = tmp >> 8;
		tmp = device->eeprom_read(1);
		device->netdev->mac_addr[2] = tmp & 0xFF;
		device->netdev->mac_addr[3] = tmp >> 8;
		tmp = device->eeprom_read(2);
		device->netdev->mac_addr[4] = tmp & 0xFF;
		device->netdev->mac_addr[5] = tmp >> 8;
	}
	else
	{
		uint32_t ral = device->cmd_read32(E1000_REG_RXADDR);
		uint32_t rah = device->cmd_read32(E1000_REG_RXADDR_HIGH);

		device->netdev->mac_addr[0] = ral & 0xFF;
		device->netdev->mac_addr[1] = (ral >> 8) & 0xFF;
		device->netdev->mac_addr[2] = (ral >> 16) & 0xFF;
		device->netdev->mac_addr[3] = (ral >> 24) & 0xFF;
		device->netdev->mac_addr[4] = rah & 0xFF;
		device->netdev->mac_addr[5] = (rah >> 8) & 0xFF;
	}
}

void e1000_reset(e1000_device* device)
{
	device->cmd_write32(E1000_REG_IMC, 0xFFFFFFFF);
	device->cmd_write32(E1000_REG_ICR, 0xFFFFFFFF);
	auto status = device->cmd_read32(E1000_REG_STATUS);

	// fixme: turbo unsafe shared PIC on all CPUs
//	timer::sleep(100);

	device->cmd_write32(E1000_REG_RCTRL, 0);
	device->cmd_write32(E1000_REG_TCTRL, E1000_TCTL_PSP);
	status = device->cmd_read32(E1000_REG_STATUS);

//	timer::sleep(100);

	auto ctrl  = device->cmd_read32(E1000_REG_CTRL);
	ctrl |= E1000_CTRL_RST;
	device->cmd_write32(E1000_REG_CTRL, ctrl);

//1	timer::sleep(100);

	device->cmd_write32(E1000_REG_IMC, 0xFFFFFFFF);
	device->cmd_write32(E1000_REG_ICR, 0xFFFFFFFF);
	status = device->cmd_read32(E1000_REG_STATUS);
}

void e1000_set_link_up(e1000_device* device)
{
	auto ctrl = device->cmd_read32(E1000_REG_CTRL);
	ctrl |= E1000_CTRL_SLU | E1000_CTRL_SPD_1000;
	ctrl &= ~(E1000_CTRL_LRST | E1000_CTRL_PHY_RST);
	device->cmd_write32(E1000_REG_CTRL, ctrl);

	device->link = (device->cmd_read32(E1000_REG_STATUS) & E1000_STATUS_LU);
}

void e1000_init_rx(e1000_device* device)
{
	vm_space* kernel_vm = vm_get_kernel_space();
	device->rx_descs = (e1000_rx_desc_t*)vm_space_map(kernel_vm,
	{
		.length = sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT,
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_ALLOCATED
	});

	memset(device->rx_descs, 0, sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);

	for(int i = 0; i < E1000_RX_DESC_COUNT; i++)
	{
		device->rx_page[i] = vm_space_map(kernel_vm,
		{
			.length = 0x1000, 
			.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
			.flags = VM_FLAG_ALLOCATED
		});
		memset((void*)device->rx_page[i], 0, 0x1000);
		device->rx_descs[i].addr = vm_space_get_mapping(kernel_vm, device->rx_page[i]).base;
		device->rx_descs[i].status = 0;
	}

	auto rxd_phys = vm_space_get_mapping(kernel_vm, (virtaddr_t)device->rx_descs).base;
	device->cmd_write32(E1000_REG_RXDESC_HIGH, (uint32_t)(rxd_phys >> 32));
	device->cmd_write32(E1000_REG_RXDESC_LOW, (uint32_t)(rxd_phys & 0xFFFFFFFF));

	device->cmd_write32(E1000_REG_RXDESC_LEN, E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t));

	device->cur_rx = 0;
	device->cmd_write32(E1000_REG_RXDESC_HEAD, 0);
	device->cmd_write32(E1000_REG_RXDESC_TAIL, E1000_RX_DESC_COUNT - 1);

	device->cmd_write32(E1000_REG_RCTRL,
		E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE |
		E1000_RCTL_MPE | E1000_RCTL_BAM | E1000_RCTL_SZ_4096 |
		E1000_RCTL_SECRC | E1000_RCTL_BSEX);
}

void e1000_init_tx(e1000_device* device)
{
	vm_space* kernel_vm = vm_get_kernel_space();
	device->tx_descs = (e1000_tx_desc_t*)vm_space_map(kernel_vm,
	{
		.length = sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT,
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_ALLOCATED
	});
	memset(device->tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);

	for(int i = 0; i < E1000_TX_DESC_COUNT; i++)
	{
		device->tx_page[i] = vm_space_map(kernel_vm,
		{
			.length = 0x1000, 
			.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
			.flags = VM_FLAG_ALLOCATED
		});
		memset((void*)device->tx_page[i], 0, 0x1000);
		device->tx_descs[i].addr = vm_space_get_mapping(kernel_vm, device->tx_page[i]).base;

		device->tx_descs[i].status = 0;
		device->tx_descs[i].cmd = E1000_CMD_EOP;
	}

	auto txd_phys = vm_space_get_mapping(kernel_vm, (virtaddr_t)device->tx_descs).base;
	device->cmd_write32(E1000_REG_TXDESC_HIGH, (uint32_t)(txd_phys >> 32));
	device->cmd_write32(E1000_REG_TXDESC_LOW, (uint32_t)(txd_phys & 0xFFFFFFFF));

	device->cmd_write32(E1000_REG_TXDESC_LEN, E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t));

	device->cur_tx = 0;
	device->cmd_write32(E1000_REG_TXDESC_HEAD, 0);
	device->cmd_write32(E1000_REG_TXDESC_TAIL, 0);

	auto tctl = device->cmd_read32(E1000_REG_TCTRL);

	tctl &= (~0XFF << E1000_TCTL_CT_SHIFT);
	tctl |= (15 << E1000_TCTL_CT_SHIFT);

	tctl |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_RTLC;
	device->cmd_write32(E1000_REG_TCTRL, tctl);
}

//FIXME: lookup by IRQ
static e1000_device* nic = nullptr;

void e1000_irq()
{
	auto icr = nic->cmd_read32(E1000_REG_ICR);
	if(icr)
	{
		if(icr & E1000_ICR_RXQ0)
		{
			bool got_packet = false;
			uint32_t old_rx = 0;
			while(nic->rx_descs[nic->cur_rx].status & 0x1)
			{
				e1000_rx_desc_t* desc = nic->rx_descs + nic->cur_rx;
				desc->status = 0;

				log::debug("e1000: RX packet size {:#x}", desc->length);

				auto* eth = reinterpret_cast<ether_packet*>(nic->rx_page[nic->cur_rx]);
				ether_recv_packet(nic->netdev, eth, desc->length);

				got_packet = true;

				old_rx = nic->cur_rx;
				nic->cur_rx = (nic->cur_rx + 1) % E1000_RX_DESC_COUNT;
			}


			if(got_packet)
				nic->cmd_write32(E1000_REG_RXDESC_TAIL, old_rx);
		}
		nic->cmd_write32(E1000_REG_ICR, icr);
	}
}

ssize_t e1000_send(netdev_t* netdev, byte* buffer, size_t size)
{
	auto* dev = (e1000_device*)netdev->data;

	memcpy((byte*)dev->tx_page[dev->cur_tx], buffer, size);
	e1000_tx_desc_t* desc = nic->tx_descs + dev->cur_tx;
	desc->length = size;
	desc->cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS | E1000_CMD_RPS;
	desc->status = 0;
	desc->css = 0;
	desc->special = 0;

	dev->cur_tx++;
	if(dev->cur_tx == E1000_TX_DESC_COUNT)
		dev->cur_tx = 0;

	dev->cmd_write32(E1000_REG_TXDESC_TAIL, dev->cur_tx);

	auto status = dev->cmd_read32(E1000_REG_STATUS);

	return size;
}

static netdev_ops e1000_ops =
{
	.send = e1000_send
};

void e1000_init(pcie_device& dev)
{
	auto* device = (e1000_device*)kmalloc(sizeof(e1000_device));
	device->pcie = dev;
	device->has_eeprom = false;
	device->netdev = netdev_create();
	device->netdev->ops = &e1000_ops;
	device->netdev->data = device;
	nic = device;

	auto pcie_reg1 = device->pcie.read_config32(0x4);
	pcie_reg1 |= ((0x400 | 0x4 | 0x2) & ~(0x1)); // INTERRUPT_DISABLE | BUS_MASTER | MEMORY_SPACE | ~IO_SPACE
	device->pcie.write_config32(0x4, pcie_reg1);

	virtaddr_t bar = device->pcie.read_bar32(0);
	auto bar_size = device->pcie.get_bar32_size(0);
	log::debug("e1000: BAR0 {:#x} size {:#x}", bar, bar_size);

	vm_space* kernel_vm = vm_get_kernel_space();
	device->base_address = vm_space_map(kernel_vm,
	{
		.length = bar_size, 
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_DEVICE,
		.phys_base = bar
	});
	if(!device->base_address)
	{	
		log::error("e1000: failed to mmap BAR");
		return;
	}

	msix_descriptor_t msix;
	auto msix_status = device->pcie.enable_msix(msix);
	if(!msix_status)
	{
		log::error("e1000: PCIe device has no MSI-X capability");
		return;
	}
	
//	e1000_reset(device);
	log::debug("e1000: device reset");

	physaddr_t msi_address = 0xfee00000;
	auto irq = allocate_irq();
	install_irq_handler(irq, e1000_irq);

	log::debug("e1000: allocated irq {}", irq);

 	auto* msi_table = msix.table;
	*msi_table++ = (msi_address & ~0x3);
	*msi_table++ = (msi_address >> 32);
	*msi_table++ = (irq & 0xFF);
	*msi_table++ = 0;

	log::debug("e1000: enabled MSI-X interrupts");
	
	//e1000_detect_eeprom(device);
	e1000_read_mac(device);

	format_to(string_span{&device->netdev->name[0], 24}, "enp{}s{}", device->pcie.bus, device->pcie.device);
	log::debug("e1000: netdevice enp{}s{} MAC address {:x}:{:x}:{:x}:{:x}:{:x}:{:x}", device->pcie.bus, device->pcie.device, device->netdev->mac_addr[0], device->netdev->mac_addr[1], device->netdev->mac_addr[2], device->netdev->mac_addr[3], device->netdev->mac_addr[4], device->netdev->mac_addr[5]);


	e1000_set_link_up(device);
	log::debug("e1000: set link up: {}", device->link);
/*
	for(int i = 0; i < 128; i++)
		device->cmd_write32(E1000_REG_MTA + i * 4, 0);
	for(int i = 0; i < 64; i++)
		auto val = device->cmd_read32(E1000_REG_CRCERRS + i * 4);
*/
	e1000_init_rx(device);
	e1000_init_tx(device);

//	device->cmd_write32(E1000_REG_RDTR, 0);
//	device->cmd_write32(E1000_REG_ITR, 500);
//	auto _status = device->cmd_read32(E1000_REG_STATUS);
	device->cmd_write32(E1000_REG_IVAR, E1000_IVAR_ENTRY_VALID | (E1000_IVAR_ENTRY_VALID << 8) | (E1000_IVAR_ENTRY_VALID << 16));

	device->cmd_write32(E1000_REG_IMS, 
		E1000_ICR_TXDW | E1000_ICR_TXQE |
		E1000_ICR_LSC |
		E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_RXT0 | E1000_ICR_RXQ0 | 
		E1000_ICR_SRPD |
		E1000_ICR_ACK);
}
