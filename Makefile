#########################
# Makefile for Lyos     #
#########################

VERSION = 0
PATCHLEVEL = 3
SUBLEVEL = 2
EXTRAVERSION =

SUBARCH = $(shell uname -m | sed -e s/sun4u/sparc64/ \
		-e s/arm.*/arm/ -e s/sa110/arm/ \
		-e s/s390x/s390/ -e s/parisc64/parisc/ \
		-e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
		-e s/sh[234].*/sh/ )

ARCH ?= $(SUBARCH)

ifeq ($(ARCH),i386)
	ARCH = x86
endif

ifeq ($(ARCH),i686)
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

LDSCRIPT = $(ARCHDIR)/kernel/lyos.ld

HD		= lyos-disk.img

# Programs, flags, etc.
ASM		= nasm
DASM		= objdump
HOSTCC	= gcc
HOSTLD	= ld
CC		= $(SUBARCH)-pc-lyos-gcc -I $(INCDIR)/ -I $(ARCHINCDIR)/ -g
LD		= $(SUBARCH)-pc-lyos-ld
ASMBFLAGS	= -I $(ARCHDIR)/boot/include/
ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
MAKEFLAGS	+= --no-print-directory
LDFLAGS		= -T $(LDSCRIPT) -Map System.map 
DASMFLAGS	= -D
ARFLAGS		= rcs

export ASM CC LD

# This Program
LYOSARCHDIR	= arch/$(ARCH)
LYOSKERNEL	= $(LYOSARCHDIR)/kernel.bin
KRNLOBJ		= kernel/krnl.o
LIB			= lib/liblyos.a
LIBC		= $(SRCDIR)/toolchain/local/$(SUBARCH)-pc-lyos/lib/libc.a 
FSOBJ		= fs/fs.o
MMOBJ		= mm/mm.o
DRVOBJ		= drivers/drivers.o

OBJS		= $(KRNLOBJ) \
			$(FSOBJ) \
			$(MMOBJ) \
			$(DRVOBJ) 

DASMOUTPUT	= kernel.bin.asm

COLORDEFAULT= \033[0m
COLORRED	= \033[1;31m
COLORGREEN	= \033[1;32m
COLORYELLOW	= \033[1;33m
COLORBLUE	= \033[1;34m

# All Phony Targets
.PHONY : everything final image clean realclean disasm all buildimg mrproper help lib config menuconfig

# Default starting position
kernel : realclean everything

include $(ARCHDIR)/Makefile

everything : genconf $(LYOSKERNEL)

all : realclean everything image lib cmd

genconf:
	@echo -e '$(COLORGREEN)Generating compile.h...$(COLORDEFAULT)'
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
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	@find . -name "*.o" -exec rm -f {} \;
	@echo -e '$(COLORRED)Removing compile.h...$(COLORDEFAULT)'
	@rm -f $(INCLUDEDIR)/lyos/compile.h
	@echo -e '$(COLORRED)Removing configure files...$(COLORDEFAULT)'
	@rm -f .config .config.old

clean :
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	@rm -f $(OBJS)

realclean :
	@find . -name "*.o" -exec rm -f {} \;
	@rm -f $(LYOSBOOT) $(LYOSKERNEL) $(LIB)

disasm :
	@echo -e '$(COLORBLUE)Disassembling the kernel...$(COLORDEFAULT)'
	@echo -e '\tDASM\t$(LYOSKERNEL)'
	@$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

help :
	@echo "Make options:"
	@echo "-----------------------------------------------------------------"
	@echo "make\t\t: build the kernel image."
	@echo "make lib\t: build the Lyos C library."
	@echo "make cmd\t: install the command files to the HD."
	@echo "make disasm\t: dump the kernel into kernel.bin.asm."
	@echo "-----------------------------------------------------------------"
	@echo "make clean\t: remove all object files but keep config files."
	@echo "make mrproper\t: remove all object files and config file."

$(LYOSKERNEL) : $(OBJS) $(LIB) $(LIBC) 
	@echo -e '\tLD\t$@'
	@$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^
	@@echo -e '$(COLORGREEN)Kernel is ready.$(COLORDEFAULT)'

$(KRNLOBJ):
	@echo -e '$(COLORGREEN)Compiling the kernel...$(COLORDEFAULT)'
	@(cd kernel; make)

$(LIB):
	@echo -e '$(COLORGREEN)Compiling the library...$(COLORDEFAULT)'
	@(cd lib; make)

$(FSOBJ):
	@echo -e '$(COLORGREEN)Compiling the filesystem server...$(COLORDEFAULT)'
	@(cd fs; make)

$(MMOBJ):
	@echo -e '$(COLORGREEN)Compiling the memory management server...$(COLORDEFAULT)'
	@(cd mm; make)

$(DRVOBJ):
	@echo -e '$(COLORGREEN)Compiling device drivers...$(COLORDEFAULT)'
	@(cd drivers; make)
