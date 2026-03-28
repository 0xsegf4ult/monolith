#pragma once

#include <lib/types.hpp>

enum idt_type
{
        IDT_INTR_GATE = 0x8e,
        IDT_TRAP_GATE = 0x8f
};

struct __attribute__((packed)) idt_entry_t
{
        idt_entry_t() = default;

        idt_entry_t(uint64_t base, uint16_t sel, uint8_t gate_type, uint8_t priv)
        {
                base16_low = base & 0xFFFF;
                base16_high = (base >> 16) & 0xFFFF;
                base32_high = base >> 32;
                selector = sel;
                type = gate_type | (priv << 5);
                zero = 0;
                padding = 0;
        }

        uint16_t base16_low;
        uint16_t selector;
        uint8_t zero;
        uint8_t type;
        uint16_t base16_high;
        uint32_t base32_high;
        uint32_t padding;
};
static_assert(sizeof(idt_entry_t) == 16);

struct __attribute__((packed)) idtr_t
{
        uint16_t limit;
        idt_entry_t* base;
};
static_assert(sizeof(idtr_t) == 10);

void setup_idt();
void load_idt();

enum InterruptID
{
        DivisionError,
        Debug,
        NMI,
        Breakpoint,
        Overflow,
        BoundExceeded,
        InvalidOpcode,
        DeviceUnavailable,
        DoubleFault,
        CSegOverrun,
        InvalidTSS,
        SegmentNotPresent,
        StackSegmentFault,
        GPFault,
        PageFault,
        Reserved,
        FPUException,
        AlignmentCheck,
        MachineCheck,
        SIMDFPException,
        VirtualizationException,
        ControlProtectionException,
        HypervisorInj = 28,
        VMMCommunication = 29,
        SecurityException = 30,
};

typedef void (*dev_irq_handler_t)();

uint32_t allocate_irq();
void install_irq_handler(uint8_t irq, dev_irq_handler_t handler);
void remove_irq_handler(uint8_t irq);
