#pragma once

#include <types.hpp>

enum LAPIC_IPI : uint32_t
{
	LAPIC_NMI_IPI 		= 0x400,
	LAPIC_INIT_IPI 		= 0x500,
	LAPIC_STARTUP_IPI 	= 0x600,
};

enum LAPIC_REG : uint32_t
{
	LAPIC_REG_EOI			= 0x0b0,
	LAPIC_REG_SPURIOUS_INT		= 0x0f0,
	LAPIC_REG_ICR			= 0x300,
	LAPIC_REG_LVT_TIMER 		= 0x320,
	LAPIC_REG_TIMER_INITCNT 	= 0x380,
	LAPIC_REG_TIMER_CURRCNT 	= 0x390,
	LAPIC_REG_TIMER_DIVIDER 	= 0x3e0,
};

enum LAPIC_ICR : uint32_t
{
	LAPIC_ICR_VECTOR	= 0x000FF,
	LAPIC_ICR_DELIVERY_MODE = 0x00700,
	LAPIC_ICR_DESTINATION	= 0x00800,
	LAPIC_ICR_STATUS	= 0x01000,
	LAPIC_ICR_INIT_DEASSERT = 0x04000,
	LAPIC_ICR_DEST_TYPE	= 0x60000
};

constexpr uint32_t APIC_BASE_MSR_ENABLE = 0x800;
constexpr uint32_t LAPIC_SPURIOUS_ENABLE = 0x100;
constexpr uint32_t LAPIC_SPURIOUS_INT = 0xFF;

void lapic_init();
void lapic_enable();
void lapic_eoi();
void lapic_send_ipi(uint32_t id, uint32_t ipi);
void lapic_timer_enable();
