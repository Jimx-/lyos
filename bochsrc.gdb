###############################################################
# Configuration file for Bochs
###############################################################

# how much memory the emulated machine will have
megs: 32

# filename of ROM images
romimage: file=$BXSHARE/BIOS-bochs-latest
vgaromimage: file=$BXSHARE/VGABIOS-lgpl-latest

# hard disk
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
# !! Remember to change these if the hd img is changed:
#    1. include/sys/config.h::MINOR_BOOT
#    2. boot/include/load.inc::ROOT_BASE
#    3. Makefile::HD
#    4. commands/Makefile::HD
ata0-master: type=disk, path="lyos-disk.img", mode=flat, cylinders=520, heads=16, spt=63

# choose the boot disk.
boot: disk

# where do we send log messages?
# log: bochsout.txt

# disable the mouse
mouse: enabled=0

#pci: enabled=1, chipset=i440fx
#pcidev: vendor=0x1234, device=0x5678

# enable key mapping, using US layout as default.
keyboard_mapping: enabled=1, map=$BXSHARE/keymaps/x11-pc-us.map

#pci: enabled=1

gdbstub: enabled=1, port=1234, text_base=0, data_base=0, bss_base=0
