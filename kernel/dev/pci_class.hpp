#pragma once

#include <lib/types.hpp>

constexpr const char* class_1_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "SCSI controller";
	case 0x1:
		return "IDE controller";
	case 0x2:
		return "Floppy controller";
	case 0x3:
		return "IPI controller";
	case 0x4:
		return "RAID controller";
	case 0x5:
		return "ATA controller";
	case 0x6:
		return "SATA controller";
	case 0x7:
		return "SAS controller";
	case 0x8:
		return "NVME controller";
	default:
		return "Storage controller";
	}
}

constexpr const char* class_2_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "Ethernet controller";
	default:
		return "Network controller";
	}
}

constexpr const char* class_3_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "VGA compatible controller";
	case 0x2:
		return "3D controller";
	default:
		return "Display controller";
	}
}

constexpr const char* class_4_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0x3:
		return "Audio device";
	default:
		return "Multimedia controller";
	}
}	

constexpr const char* class_5_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "RAM controller";
	case 0x1:
		return "Flash controller";
	default:
		return "Memory controller";
	}
}

constexpr const char* class_6_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "Host bridge";
	case 0x1: 
		return "ISA bridge";
	case 0x2:
		return "EISA bridge";
	case 0x4:
	case 0x9:
		return "PCI bridge";
	default:
		return "Bridge";
	}
}

constexpr const char* class_7_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "Serial controller";
	case 0x1:
		return "Parallel controller";
	case 0x2:
		return "Multiport serial controller";
	case 0x3:
		return "Modem";
	case 0x4:
		return "GPIB controller";
	case 0x5:
		return "Smart card controller";
	default:
		return "Simple communication controller";
	}
}

constexpr const char* class_8_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "Interrupt controller";
	case 0x1:
		return "DMA controller";
	case 0x2:
		return "Timer";
	case 0x3:
		return "RTC controller";
	case 0x4:
		return "PCI hotplug controller";
	case 0x5:
		return "SD host controller";
	case 0x6:
		return "IOMMU";
	default:
		return "Base system peripheral";
	}
}

constexpr const char* class_C_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0:
		return "FireWire controller";
	case 0x3:
		return "USB controller";
	case 0x5:
		return "SMBus controller";
	case 0x7:
		return "IPMI interface";
	default:
		return "Serial bus controller";
	}
}

constexpr const char* class_D_to_string(uint8_t subclass)
{
	switch(subclass)
	{
	case 0x11:
		return "Bluetooth controller";
	case 0x20:
		return "802.11a controller";
	case 0x21:
		return "802.11b controller";
	default:
		return "Wireless controller";
	}
}

constexpr const char* class_to_string(uint8_t class_code, uint8_t subclass)
{
	switch(class_code)
	{
	case 0x1:
		return class_1_to_string(subclass);
	case 0x2:
		return class_2_to_string(subclass);
	case 0x3:
		return class_3_to_string(subclass);
	case 0x4:
		return class_4_to_string(subclass);
	case 0x5:
		return class_5_to_string(subclass);
	case 0x6:
		return class_6_to_string(subclass);
	case 0x7:
		return class_7_to_string(subclass);
	case 0x8:
		return class_8_to_string(subclass);
	case 0x9:
		return "Input device controller";
	case 0xA:
		return "Docking station";
	case 0xB:
		return "Processor";
	case 0xC:
		return class_C_to_string(subclass);
	case 0xD:
		return class_D_to_string(subclass);
	case 0xE:
		return "Intelligent controller";
	case 0xF:
		return "Satellite communication controller"; 
	case 0x10:
		return "Encryption controller";
	case 0x11:
		return "Signal processing controller";
	case 0x12:
		return "Processing accelerator";
	case 0x13:
		return "Non-Essential instrumentation";
	case 0x40:
		return "Co-Processor";
	default:
		return nullptr;
	}
}
