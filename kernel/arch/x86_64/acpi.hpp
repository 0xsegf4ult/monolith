#pragma once

#include <lib/types.hpp>

namespace acpi
{

struct __attribute__((packed)) rsdp_v1
{
        char signature[8];
        uint8_t checksum;
        char oemid[6];
        uint8_t revision;
        uint32_t rsdt_address;
};

struct __attribute__((packed)) rsdp_v2 : public rsdp_v1
{
        uint32_t length;
        uint64_t xsdt_address;
        uint8_t extended_checksum;
        uint8_t reserved[3];
};

struct __attribute__((packed)) sdt_header
{
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oemid[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_version;
};

struct __attribute__((packed)) madt  : public sdt_header
{
        uint32_t lapic_address;
        uint32_t flags;
};

enum class MADTEntryType : uint8_t
{
        LAPIC,
        IOAPIC,
        IOAPIC_ISO,
        IOAPIC_NMI_SOURCE,
        LAPIC_NMI,
        LAPIC_AO,
        LOCAL_X2APIC = 9
};

struct __attribute__((packed)) madt_entry
{
        MADTEntryType entry_type;
        uint8_t entry_length;
};

struct __attribute__((packed)) madt_lapic_entry : public madt_entry
{
        uint8_t acpi_cpuid;
        uint8_t apic_id;
        uint32_t flags;
};

struct __attribute__((packed)) madt_ioapic_entry : public madt_entry
{
        uint8_t ioapic_id;
        uint8_t reserved;
        uint32_t ioapic_address;
        uint32_t gsi_base;
};

struct __attribute__((packed)) madt_ioapic_iso_entry : public madt_entry
{
        uint8_t bus_source;
        uint8_t irq_source;
        uint32_t gsi;
        uint16_t flags;
};

struct __attribute__((packed)) madt_ioapic_nmi_source_entry : public madt_entry
{
        uint8_t nmi_source;
        uint8_t reserved;
        uint16_t flags;
        uint32_t gsi;
};

struct __attribute__((packed)) madt_lapic_nmi_entry : public madt_entry
{
        uint8_t acpi_cpuid;
        uint16_t flags;
        uint8_t lint;
};

struct __attribute__((packed)) madt_lapic_override_entry : public madt_entry
{
        uint16_t reserved;
        uint64_t lapic_address64;
};

struct __attribute__((packed)) madt_x2apic_entry : public madt_entry
{
        uint16_t reserved;
        uint32_t x2apic_id;
        uint32_t flags;
        uint32_t acpi_cpuid;
};

struct __attribute__((packed)) acpi_gas
{
        uint8_t address_space;
        uint8_t bit_width;
        uint8_t bit_offset;
        uint8_t access_size;
        uint64_t address;
};

struct __attribute__((packed)) fadt : public sdt_header
{
        uint32_t firmware_ctrl;
        uint32_t dsdt;

        uint8_t reserved;

        uint8_t preferred_pmprofile;
        uint16_t sci_interrupt;
        uint32_t smi_commandport;
        uint8_t acpi_enable;
        uint8_t acpi_disable;
        uint8_t s4bios_req;
        uint8_t pstate_control;
        uint32_t pm1a_event_block;
        uint32_t pm1b_event_block;
        uint32_t pm1a_control_block;
        uint32_t pm1b_control_block;
        uint32_t pm2_control_block;
        uint32_t pm_timer_block;
        uint32_t gpe0_block;
        uint32_t gpe1_block;
        uint8_t pm1_event_length;
        uint8_t pm1_control_length;
        uint8_t pm2_control_length;
        uint8_t pm_timer_length;
        uint8_t gpe0_length;
        uint8_t gpe1_length;
        uint8_t gpe1_base;
        uint8_t cstate_control;
        uint16_t worst_c2_latency;
        uint16_t worst_c3_latency;
        uint16_t flush_size;
        uint16_t flush_stride;
        uint8_t duty_offset;
        uint8_t duty_width;
        uint8_t day_alarm;
        uint8_t month_alarm;
        uint8_t century;

        uint16_t boot_architecture_flags;

        uint8_t reserved2;
        uint32_t flags;

        acpi_gas reset_reg;

	uint8_t reset_value;
        uint8_t reserved3[3];

        uint64_t x_firmwarecontrol;
        uint64_t x_dsdt;

        acpi_gas x_pm1a_event_block;
        acpi_gas x_pm1b_event_block;
        acpi_gas x_pm1a_control_block;
        acpi_gas x_pm1b_control_block;
        acpi_gas x_pm2_control_block;
        acpi_gas x_pm_timer_block;
        acpi_gas x_gpe0_block;
        acpi_gas x_gpe1_block;
};

struct acpi_tables
{
	const rsdp_v2* xsdp;
	const sdt_header* xsdt;
	const fadt* fadt;
	const madt* madt;
};

const sdt_header* find_table(const sdt_header* root_table, const char* id);
acpi_tables parse_tables(const rsdp_v1* rsdp);
void parse_madt(const madt* table);

}
