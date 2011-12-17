#########################
# Makefile for Lyos     #
#########################

VERSION = 0
PATCHLEVEL = 2
SUBLEVEL = 30
EXTRAVERSION =

ARCH = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
				  -e s/arm.*/arm/ -e s/sa110/arm/ \
				  -e s/s390x/s390/ -e s/parisc64/parisc/ \
				  -e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
				  -e s/sh[234].*/sh/ )
ifeq ($(ARCH),i386)
	ARCH = x86
endif

export ARCH

KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
export KERNELVERSION

# Entry point of Lyos
# It must have the same value with 'KernelEntryPointPhyAddr' in load.inc!
ENTRYPOINT	= 0x1000

FD		= a.img
HD		= 80m.img

# Programs, flags, etc.
ASM		= nasm
DASM		= objdump
CC		= gcc
LD		= ld
ASMBFLAGS	= -I boot/include/
ASMKFLAGS	= -I include/ -I include/sys/ -f elf
#CFLAGS		= -I include/ -I include/sys/ -c -fno-builtin -Wall
CFLAGS		= -I include/ -I include/sys/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
LDFLAGS		= -Ttext $(ENTRYPOINT) -Map krnl.map
DASMFLAGS	= -D
ARFLAGS		= rcs

# This Program
LYOSARCHDIR	= arch/$(ARCH)
LYOSBOOTDIR	= $(LYOSARCHDIR)/boot
LYOSBOOT	= $(LYOSBOOTDIR)/boot.bin $(LYOSBOOTDIR)/hdboot.bin $(LYOSBOOTDIR)/loader.bin \
			$(LYOSBOOTDIR)/hdloader.bin
LYOSKERNEL	= kernel.bin
KRNLOBJ		= kernel/krnl.o
LIB		= lib/lyoscrt.a
FSOBJ		= fs/fs.o
MMOBJ		= mm/mm.o
DRVOBJ		= drivers/drivers.o

OBJS		= $(KRNLOBJ) \
			$(FSOBJ) \
			$(MMOBJ) \
			$(DRVOBJ) \
			lib/syslog.o

DASMOUTPUT	= kernel.bin.asm

# All Phony Targets
.PHONY : everything final image clean realclean disasm all buildimg mrproper help lib

# Default starting position
help :
	@echo "make image 	: build the kernel image."
	@echo "make cmd   	: install the command files to the HD."
	@echo "make disasm	: dump the kernel into kernel.bin.asm."
	@echo "make mrproper	: remove all object files."

everything : genconf $(LYOSBOOT) $(LYOSKERNEL)

all : realclean everything image lib cmd

image : realclean everything clean buildimg

genconf:
	$(shell ./scripts/gencompile.sh $(ARCH) $(KERNELVERSION) $(CC))

cmd :
	(cd command;make install)

lib :
	(cd lib; make)

mrproper:
	find . -name "*.o" -exec rm -fv {} \;

clean :
	rm -f $(OBJS) $(LOBJS)

realclean :
	rm -f $(OBJS) $(LOBJS) $(LIB) $(LYOSBOOT) $(LYOSKERNEL)

disasm :
	$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

buildimg :
	dd if=$(LYOSBOOTDIR)/boot.bin of=$(FD) bs=512 count=1 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200" | bc` bs=1 count=446 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200+1FE" | bc` skip=510 bs=1 count=2 conv=notrunc
	sudo mount -o loop $(FD) /mnt/floppy/
	sudo cp -fv $(LYOSBOOTDIR)/loader.bin /mnt/floppy/
	sudo cp -fv kernel.bin /mnt/floppy
	sudo umount /mnt/floppy

$(LYOSBOOT):
	(cd $(LYOSBOOTDIR); make)

$(LYOSKERNEL) : $(OBJS) $(LIB)
	$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^

$(KRNLOBJ):
	(cd kernel; make)

$(LIB):
	(cd lib; make)

$(FSOBJ):
	(cd fs; make)

$(MMOBJ):
	(cd mm; make)

$(DRVOBJ):
	(cd drivers; make)

lib/syslog.o: lib/syslog.c
	$(CC) $(CFLAGS) -o $@ $<
