#########################
# Makefile for Lyos     #
#########################

VERSION = 0
PATCHLEVEL = 4
SUBLEVEL = 1
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

# Import configuration
ifeq ($(wildcard .config),) 
else
	include .config
endif

KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
export KERNELVERSION

# Directories
SRCDIR = $(shell pwd)
INCDIR = $(SRCDIR)/include
SYSINCDIR = $(SRCDIR)/include/sys
ARCHINCDIR = $(SRCDIR)/include/arch/$(ARCH)
LIBDIR = $(SRCDIR)/lib/
ARCHDIR = $(SRCDIR)/arch/$(ARCH)
ARCHINC = $(ARCHDIR)/include
ARCHLIB = $(ARCHDIR)/lib
BINDIR ?= $(SRCDIR)/sysroot
LIBOUTDIR = $(BINDIR)/lib
PATH := $(SRCDIR)/toolchain/local/bin:$(PATH)
export SRCDIR INCDIR SYSINCDIR ARCHINCDIR LIBDIR ARCHDIR BINDIR LIBOUTDIR ARCHINC ARCHLIB PATH

LDSCRIPT = kernel/arch/$(ARCH)/lyos.ld

HD		= lyos-disk.img

# Programs, flags, etc.
HOSTCC	= gcc
HOSTLD	= ld
AS 		= $(SUBARCH)-elf-lyos-as
CC		= $(SUBARCH)-elf-lyos-gcc
LD		= $(SUBARCH)-elf-lyos-ld
CFLAGS		= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -L$(LIBOUTDIR)/ -c -fno-builtin -fno-stack-protector -fpack-struct -Wall
SERVERCFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -L$(LIBOUTDIR)/ -Wall -static
MAKEFLAGS	+= --no-print-directory
LDFLAGS		= -T $(LDSCRIPT) -Map $(ARCHDIR)/System.map
ARFLAGS		= rcs
MAKE 		= make

ifeq ($(CONFIG_DEBUG_INFO),y)
	CFLAGS += -g
	SERVERCFLAGS += -g
endif

export ASM CC LD CFLAGS HOSTCC HOSTLD SERVERCFLAGS

# This Program
LYOSARCHDIR	= arch/$(ARCH)
LYOSKERNEL	= $(LYOSARCHDIR)/lyos.bin

ifeq ($(CONFIG_COMPRESS_GZIP),y)
	ZIP = gzip
	LYOSZKERNEL = $(LYOSARCHDIR)/lyos.gz
endif

KRNLOBJ		= kernel/krnl.o
LIB			= $(LIBOUTDIR)/liblyos.a
LIBC		= $(SRCDIR)/toolchain/local/$(SUBARCH)-elf-lyos/lib/libc.a 
FSOBJ		= fs/fs.o
MMOBJ		= mm/mm.o
DRVOBJ		= drivers/drivers.o
SERVMANOBJ  = servman/servman.o
LYOSINIT	= init/init
LYOSINITRD	= $(ARCHDIR)/initrd.tar

ifeq ($(ARCH),x86)
OBJS		= $(KRNLOBJ) \
			$(FSOBJ) \
			$(MMOBJ) \
			$(DRVOBJ) \
			$(SERVMANOBJ)
else
OBJS = $(KRNLOBJ)
endif

DASMOUTPUT	= lyos.bin.asm

COLORDEFAULT= \033[0m
COLORRED	= \033[1;31m
COLORGREEN	= \033[1;32m
COLORYELLOW	= \033[1;33m
COLORBLUE	= \033[1;34m

CONFIGDIR = $(SRCDIR)/include/config
CONFIGIN = $(SRCDIR)/config.in
CONF = $(SRCDIR)/scripts/config/conf
MCONF = $(SRCDIR)/scripts/config/mconf
CONFIGINC = $(SRCDIR)/.config
AUTOCONFINC = $(SRCDIR)/include/config/autoconf.h

KCONFIG_AUTOHEADER = include/config/autoconf.h
export KCONFIG_AUTOHEADER

# All Phony Targets
.PHONY : everything final image clean realclean disasm all buildimg help lib config menuconfig
	setup-toolchain libraries mrproper

# Default starting position
kernel : realclean everything

include $(ARCHDIR)/Makefile

everything : $(CONFIGINC) $(AUTOCONFINC) genconf libraries $(LYOSKERNEL) $(LYOSINIT) initrd

all : realclean everything image

setup-toolchain:
	@echo -e '$(COLORGREEN)Setting up toolchain...$(COLORDEFAULT)'
	@./scripts/setup-toolchain.sh

$(CONFIGINC):
	@echo -e '$(COLORYELLOW)Using default configuration$(COLORDEFAULT)'
	@cp $(ARCHDIR)/configs/$(SUBARCH).conf $(CONFIGINC)

$(AUTOCONFINC):
	@$(MAKE) -f Makefile silentoldconfig

genconf:
	@echo -e '$(COLORGREEN)Generating compile.h...$(COLORDEFAULT)'
	@echo -e '\tGEN\tcompile.h'
	@sh ./scripts/genversion.sh
	@$(shell ./scripts/gencompile.sh $(ARCH) $(KERNELVERSION) $(CC) $(CONFIG_LOCALVERSION) $(CONFIG_LOCALVERSION_AUTO) $(CONFIG_SMP))

config: $(CONFIGIN) $(CONFIGINC)
	@(cd scripts/config; make config)
	$(CONF) $(CONFIGIN)
	@$(MAKE) -f Makefile silentoldconfig

menuconfig: $(CONFIGIN) $(CONFIGINC)
	@(cd scripts/config; make menuconfig) 
	$(MCONF) $(CONFIGIN)
	@$(MAKE) -f Makefile silentoldconfig

silentoldconfig:
	@rm -rf $(CONFIGDIR)
	@mkdir $(CONFIGDIR)
	@(cd scripts/config; make config)
	$(CONF) --silentoldconfig  $(CONFIGIN)

clean :
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	@rm -f $(OBJS)

realclean :
	@find . -path ./toolchain -prune -o -name "*.o" -exec rm -f {} \;
	@find . -path ./toolchain -prune -o -name "*.a" -exec rm -f {} \;
	@rm -f $(LYOSBOOT) $(LYOSKERNEL) $(LYOSZKERNEL) $(LYOSINIT) $(LYOSINITRD)

mrproper:
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	@find . -path ./toolchain -prune -o -name "*.o" -exec rm -f {} \;
	@echo -e '$(COLORRED)Removing compile.h...$(COLORDEFAULT)'
	@rm -f $(INCLUDEDIR)/lyos/compile.h
	@echo -e '$(COLORRED)Removing configure files...$(COLORDEFAULT)'
	@rm -f .config .config.old
	@rm -rf $(CONFIGDIR)

update-disk:
	@(cd userspace; make)
	@sudo bash scripts/update-disk.sh

kvm:
	@qemu-system-i386 -smp 2 -net nic,model=rtl8139 -net user lyos-disk.img -m 1024

setup-disk:
	@(cd userspace; make)
	@sudo bash scripts/setup-disk.sh

initrd:
	@echo -e '$(COLORGREEN)Making initrd...$(COLORDEFAULT)'
	@cp sysroot/sbin/init ramdisk/sbin/
	@cp sysroot/sbin/ata ramdisk/sbin/
	@cp sysroot/usr/bin/getty ramdisk/usr/bin/getty
	@cp sysroot/usr/bin/login ramdisk/usr/bin/login
	@touch ramdisk/.root
	@(cd scripts ; bash create-ramdisk-dev.sh)
	@(cd userspace; make)
	@(cd ramdisk ; tar -cvf $(LYOSINITRD) .root sbin/* dev/* etc/* usr/bin/* > /dev/null)
	@rm ramdisk/.root

disasm :
	@echo -e '$(COLORBLUE)Disassembling the kernel...$(COLORDEFAULT)'
	@echo -e '\tDASM\t$(LYOSKERNEL)'
	@$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

help :
	@echo "Make options:"
	@echo "-----------------------------------------------------------------"
	@echo "make\t\t: build the kernel image."
	@echo "make disasm\t: dump the kernel into lyos.bin.asm."
	@echo "-----------------------------------------------------------------"
	@echo "make clean\t: remove all object files but keep config files."
	@echo "make realclean\t: remove all object files and config file."

libraries:
	@echo -e '$(COLORGREEN)Compiling the libraries...$(COLORDEFAULT)'
	@(cd lib; make)

$(LYOSKERNEL) : $(OBJS) $(LIB) $(LIBC) 
	@echo -e '\tLD\t$@'
	@$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^
ifeq ($(CONFIG_COMPRESS_GZIP),y)
	@@echo -e '$(COLORGREEN)Compressing the kernel...$(COLORDEFAULT)'
	@echo -e '\tZIP\t$@'
	@$(ZIP) -cfq $(LYOSKERNEL) > $(LYOSZKERNEL)
endif
	@@echo -e '$(COLORGREEN)Kernel is ready.$(COLORDEFAULT)'

$(KRNLOBJ):
	@echo -e '$(COLORGREEN)Compiling the kernel...$(COLORDEFAULT)'
	@(cd kernel; make)

$(LIB):
	@echo -e '$(COLORGREEN)Compiling the library...$(COLORDEFAULT)'
	@(cd lib/liblyos; make)

$(FSOBJ):
	@echo -e '$(COLORGREEN)Compiling the filesystem server...$(COLORDEFAULT)'
	@(cd fs; make)

$(MMOBJ):
	@echo -e '$(COLORGREEN)Compiling the memory management server...$(COLORDEFAULT)'
	@(cd mm; make)

$(DRVOBJ):
	@echo -e '$(COLORGREEN)Compiling device drivers...$(COLORDEFAULT)'
	@(cd drivers; make)

$(SERVMANOBJ):
	@echo -e '$(COLORGREEN)Compiling service manager...$(COLORDEFAULT)'
	@(cd servman; make)

$(LYOSINIT):
	@echo -e '$(COLORGREEN)Compiling init...$(COLORDEFAULT)'
	@(cd init; make)
