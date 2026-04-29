#!/bin/bash

if [ -z "$1" ] 
then qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -serial stdio -bios /usr/share/ovmf/x64/OVMF.4m.fd -cpu max -smp cores=6 -m 256M -usb boot.img
fi

if [ "$1" = "debug" ] 
then qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -cpu max -smp cores=6 -bios /usr/share/ovmf/x64/OVMF.4m.fd -usb boot.img -serial mon:stdio -m 256M -gdb tcp::26000 -S -nographic 
fi

if [ "$1" = "full" ]
then qemu-system-x86_64 -accel kvm -machine type=q35 -cpu host -smp cores=6 -m 4096M -bios /usr/share/ovmf/x64/OVMF.4m.fd -vga virtio -serial stdio -usb boot.img
fi
