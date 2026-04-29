#pragma once

#include <lib/types.hpp>

struct pcie_device
{
	uint8_t bus;
	uint8_t device;
	uint8_t function;

	uint32_t read_config32(uint32_t offset) const;
	uint64_t read_config64(uint32_t offset) const;

	void write_config32(uint32_t offset, uint32_t data);
	void write_config64(uint32_t offset, uint64_t data);
	
	uint16_t vendor_id() const;
	uint16_t device_id() const;
	uint8_t class_code() const;
	uint8_t subclass_code() const;
	uint8_t header_type() const;
	bool is_valid() const;
	bool is_bridge() const;
	uint8_t sub_bus() const;
	bool has_multiple_functions() const;

	uint64_t read_bar() const;
	size_t get_bar_size();
};

namespace pcie
{

void set_base(physaddr_t base);
void enumerate();

}
