#pragma once

#include <lib/types.hpp>

namespace lapic
{

void set_base(physaddr_t base);
void enable();
void eoi();

}

namespace ioapic
{

class ioapic_instance
{
public:
	void enable(physaddr_t address, uint32_t gsi);
	uint64_t read_redirection_entry(uint8_t id);
	void write_redirection_entry(uint8_t id, uint64_t entry);
private:
	constexpr uint32_t mmio_read(uint8_t offset);
	constexpr void mmio_write(uint8_t offset, uint32_t value);

	virtaddr_t base_address{0};
	uint32_t gsi_base{0};

	uint8_t id{0};
	uint8_t ver{0};
	uint8_t redir_entry_count{0};
};

ioapic_instance& get(uint32_t id);

}
