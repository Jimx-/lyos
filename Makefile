#########################
# Makefile for Lyos     #
#########################

VERSION = 0
PATCHLEVEL = 3
SUBLEVEL = 2
EXTRAVERSION =

SUBARCH = $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
		-e s/arm.*/arm/ -e s/sa110/arm/ \
		-e s/s390x/s390/ -e s/parisc64/parisc/ \
		-e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
		-e s/sh[234].*/sh/ )

ARCH ?= $(SUBARCH)

ifeq ($(ARCH),i386)
	ARCH = x86
endif

export SUBARCH ARCH 

KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
export KERNELVERSION

# Directories
SRCDIR = $(shell pwd)
INCDIR = $(SRCDIR)/include
SYSINCDIR = $(SRCDIR)/include/sys
ARCHINCDIR = $(SRCDIR)/arch/$(ARCH)/include
LIBDIR = $(SRCDIR)/lib/
ARCHDIR = $(SRCDIR)/arch/$(ARCH)
ARCHINC = $(ARCHDIR)/include
ARCHLIB = $(ARCHDIR)/lib
export SRCDIR INCDIR SYSINCDIR ARCHINCDIR LIBDIR ARCHDIR ARCHINC ARCHLIB

# Entry point of Lyos
# It must have the same value with 'KernelEntryPointPhyAddr' in load.inc!
ENTRYPOINT	= 0x1000

FD		= a.img
HD		= 80m.img

# Programs, flags, etc.
ASM		= nasm
DASM		= objdump
CC		= gcc -I $(INCDIR)/ -I $(ARCHINCDIR)/
LD		= ld
ASMBFLAGS	= -I $(ARCHDIR)/boot/include/
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
MAKEFLAGS	+= --no-print-directory
LDFLAGS		= -Ttext $(ENTRYPOINT) -Map System.map
DASMFLAGS	= -D
ARFLAGS		= rcs

export ASM CC LD

# This Program
LYOSARCHDIR	= arch/$(ARCH)
LYOSKERNEL	= $(LYOSARCHDIR)/kernel.bin
KRNLOBJ		= kernel/krnl.o
LIB		= lib/liblyos.a
FSOBJ		= fs/fs.o
MMOBJ		= mm/mm.o
DRVOBJ		= drivers/drivers.o

OBJS		= $(KRNLOBJ) \
			$(FSOBJ) \
			$(MMOBJ) \
			$(DRVOBJ) \
			lib/misc/syslog.o

DASMOUTPUT	= kernel.bin.asm

# All Phony Targets
.PHONY : everything final image clean realclean disasm all buildimg mrproper help lib config menuconfig

# Default starting position
kernel : realclean everything clean

include $(ARCHDIR)/Makefile

everything : genconf $(LYOSKERNEL) $(LYOSBOOT) 

all : realclean everything image lib cmd

genconf:
	@echo -e '\tGEN\tcompile.h'
	@$(shell ./scripts/gencompile.sh $(ARCH) $(KERNELVERSION) $(CC))

CONFIGIN = $(SRCDIR)/config.in
CONF = $(SRCDIR)/scripts/config/conf
MCONF = $(SRCDIR)/scripts/config/mconf

config:
	@(cd scripts/config; make config)
	$(CONF) $(CONFIGIN)

menuconfig:
	@(cd scripts/config; make menuconfig) 
	$(MCONF) $(CONFIGIN)

cmd :
	@(cd command;make install)

lib :
	@(cd lib; make)

mrproper:
	@find . -name "*.o" -exec rm -fv {} \;
	@rm -f $(INCLUDEDIR)/lyos/compile.h
	@rm -f .config .config.old

clean :
	@rm -f $(OBJS)

realclean :
	@find . -name "*.o" -exec rm -f {} \;
	@rm -f $(LIB) $(LYOSBOOT) $(LYOSKERNEL)

disasm :
	@echo -e '\tDASM\t$(LYOSKERNEL)'
	@$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

help :
	@echo "Make options:"
	@echo "-----------------------------------------------------------------"
	@echo "make		: build the kernel image."
	@echo "make image 	: build the floppy image."
	@echo "make lib		: build the Lyos C library."
	@echo "make cmd   	: install the command files to the HD."
	@echo "make disasm	: dump the kernel into kernel.bin.asm."
	@echo "-----------------------------------------------------------------"
	@echo "make clean	: remove all object files but keep config files."
	@echo "make mrproper	: remove all object files and config file."

$(LYOSKERNEL) : $(OBJS) $(LIB)
	@echo -e '\tLD\t$@'
	@$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^

$(KRNLOBJ):
	@(cd kernel; make)

$(LIB):
	@(cd lib; make)

$(FSOBJ):
	@(cd fs; make)

$(MMOBJ):
	@(cd mm; make)

$(DRVOBJ):
	@(cd drivers; make)

lib/misc/syslog.o: lib/misc/syslog.c
	@echo -e '\tCC\tlib/misc/$@'
	@$(CC) $(CFLAGS) -o $@ $<
