#!/bin/bash

if [ -z "$1" ] 
then qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -serial stdio -bios /usr/share/ovmf/x64/OVMF.4m.fd -cpu max -smp cores=6 -m 1024M -drive file=boot.img,if=none,id=boot -device nvme,serial=qemunvme,drive=boot
fi

if [ "$1" = "debug" ] 
then qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -cpu max -smp cores=1 -bios /usr/share/ovmf/x64/OVMF.4m.fd -drive file=boot.img,if=none,id=boot -device nvme,serial=qemunvme,drive=boot -serial stdio -m 256M -gdb tcp::26000
fi

if [ "$1" = "full" ]
then qemu-system-x86_64 -accel kvm -machine type=q35 -cpu host,migratable=off -smp cores=6 -m 4096M -bios /usr/share/ovmf/x64/OVMF.4m.fd -vga virtio -serial stdio -drive file=boot.img,if=none,id=boot -device nvme,serial=qemunvme,drive=boot
fi
