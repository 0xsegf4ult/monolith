#pragma once

#include <lib/types.hpp>

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

void install_irq_handler(uint8_t irq, dev_irq_handler_t handler);
void remove_irq_handler(uint8_t irq);

struct cpu_context_t;
