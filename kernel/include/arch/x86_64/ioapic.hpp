#pragma once

#include <stdint.h>

void ioapic_init();
void ioapic_write_redirection_entry(uint8_t id, uint64_t entry);
