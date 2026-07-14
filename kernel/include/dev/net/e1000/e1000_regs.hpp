#pragma once

#include <types.hpp>

enum e1000_register : uint16_t
{
	E1000_REG_CTRL 		= 0x0000, // Device control
	E1000_REG_STATUS 	= 0x0008, // Device status
	E1000_REG_EEPROM 	= 0x0014, // EEPROM read
	E1000_REG_ICR 		= 0x00C0, // Interrupt Cause Read
	E1000_REG_ITR		= 0x00C4, // Interrupt Throttling Rate
	E1000_REG_IMS		= 0x00D0, // Interrupt Mask Set
	E1000_REG_IMC 		= 0x00D8, // Interrupt Mask Clear
	E1000_REG_IVAR		= 0x00E4, // Interrupt Vector Allocation 
	E1000_REG_RCTRL 	= 0x0100, // RX control
	E1000_REG_TCTRL 	= 0x0400, // TX control
	E1000_REG_TIPG		= 0x0410, // TX inter packet gap
	E1000_REG_RXDESC_LOW	= 0x2800, // RX descriptor address low32
	E1000_REG_RXDESC_HIGH	= 0x2804, // RX descriptor address high32
	E1000_REG_RXDESC_LEN	= 0x2808, // RX descriptor length
	E1000_REG_RXDESC_HEAD	= 0x2810, // RX descriptor head
	E1000_REG_RXDESC_TAIL	= 0x2818, // RX descriptor tail
	E1000_REG_RDTR		= 0x2820, // RX delay timer
	E1000_REG_TXDESC_LOW	= 0x3800, // TX descriptor address low32
	E1000_REG_TXDESC_HIGH	= 0x3804, // TX descriptor address high32
	E1000_REG_TXDESC_LEN	= 0x3808, // TX descriptor length
	E1000_REG_TXDESC_HEAD	= 0x3810, // TX descriptor head
	E1000_REG_TXDESC_TAIL	= 0x3818, // TX descriptor tail 
	E1000_REG_CRCERRS 	= 0x4000, // CRC error count, next 63 registers are counters
	E1000_REG_MTA		= 0x5200, // Multicast table array
	E1000_REG_RXADDR 	= 0x5400, // RX address
	E1000_REG_RXADDR_HIGH 	= 0x5404
};

// Device Control
enum e1000_ctrl : uint32_t
{
	E1000_CTRL_LRST		= 0x00000008, // link reset
	E1000_CTRL_SLU 		= 0x00000040, // set link up
	E1000_CTRL_SPD_1000	= 0x00000200, // force 1Gb
	E1000_CTRL_RST 		= 0x04000000, // reset
	E1000_CTRL_PHY_RST	= 0x80000000, // PHY reset
};

enum e1000_status : uint32_t
{
	E1000_STATUS_LU		= 0x00000002, // link up
};

enum e1000_icr : uint32_t
{
	E1000_ICR_TXDW		= 0x00000001, // TX descriptor written back
	E1000_ICR_TXQE		= 0x00000002, // TX queue empty
	E1000_ICR_LSC		= 0x00000004, // Link status change
	E1000_ICR_RXDMT0	= 0x00000010, // RX descriptor min. threshold
	E1000_ICR_RXO		= 0x00000040, // RX overrun
	E1000_ICR_RXT0		= 0x00000080, // RX timer interrupt
	E1000_ICR_SRPD		= 0x00010000, // Small RX packet detected
	E1000_ICR_RXQ0		= 0x00100000, // RX Queue 0 interrupt
	E1000_ICR_ACK		= 0x00020000, // ACK frame
};

// RX Control
enum e1000_rctl : uint32_t
{
	E1000_RCTL_RST		= 0x00000001, // software reset
	E1000_RCTL_EN		= 0x00000002, // enable
	E1000_RCTL_SBP		= 0x00000004, // store bad packet
	E1000_RCTL_UPE		= 0x00000008, // unicast promiscuous enable
	E1000_RCTL_MPE		= 0x00000010, // multicast promiscuous enable
	E1000_RCTL_BAM		= 0x00008000, // broadcast enable,
	E1000_RCTL_SZ_4096	= 0x00030000, // RX buffer 4096, requires BSEX=1
	E1000_RCTL_BSEX		= 0x02000000, // buffer size extension
	E1000_RCTL_SECRC	= 0x04000000, // strip Ethernet CRC
};

// TX Control
enum e1000_tctl : uint32_t
{
	E1000_TCTL_RST		= 0x00000001, // software reset
	E1000_TCTL_EN 		= 0x00000002, // enable 
	E1000_TCTL_PSP 		= 0x00000008, // pad short packets
	E1000_TCTL_RTLC		= 0x01000000, // Retransmit on late collision
};

enum e1000_cmd: uint8_t
{
	E1000_CMD_EOP 		= 0x01, // end of packet
	E1000_CMD_IFCS		= 0x02, // insert FCS
	E1000_CMD_RS		= 0x08, // report status
	E1000_CMD_RPS		= 0x10  // report packet sent
};

constexpr size_t E1000_TCTL_CT_SHIFT = 0x4;
constexpr uint32_t E1000_IVAR_ENTRY_VALID = 0x8;
