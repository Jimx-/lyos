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

ifeq ($(ARCH),arm)
	SUBARCH = arm
endif

ifeq ($(ARCH),riscv32)
	ARCH = riscv
endif

ifeq ($(ARCH),riscv64)
	ARCH = riscv
endif

export SUBARCH ARCH

# Import configuration
ifeq ($(wildcard .config),)
else
	-include .config
	export
endif

KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)
export KERNELVERSION

# Directories
SRCDIR = $(shell pwd)
INCDIR = $(SRCDIR)/include
SERVERSINCDIR = $(SRCDIR)/servers
SYSINCDIR = $(SRCDIR)/include/sys
ARCHINCDIR = $(SRCDIR)/include/arch/$(ARCH)
LIBDIR = $(SRCDIR)/lib/
ARCHDIR = $(SRCDIR)/arch/$(ARCH)
ARCHINC = $(ARCHDIR)/include
ARCHLIB = $(ARCHDIR)/lib
OBJDIR ?= $(SRCDIR)/obj
DESTDIR	?= $(OBJDIR)/destdir.$(ARCH)
LIBOUTDIR = $(DESTDIR)/lib
PATH := $(SRCDIR)/toolchain/local/bin:$(PATH)
export SRCDIR INCDIR SYSINCDIR ARCHINCDIR LIBDIR ARCHDIR DESTDIR BINDIR LIBOUTDIR ARCHINC ARCHLIB PATH

HD		= lyos-disk.img

# Programs, flags, etc.
HOSTCC	= gcc
HOSTLD	= ld
AS		= $(SUBARCH)-elf-lyos-as
AR		= $(SUBARCH)-elf-lyos-ar
CC		= $(SUBARCH)-elf-lyos-gcc
CPP	= $(SUBARCH)-elf-lyos-g++
LD		= $(SUBARCH)-elf-lyos-ld
OBJCOPY = $(SUBARCH)-elf-lyos-objcopy
INSTALL = install
CFLAGS	= -I $(INCDIR)/ -I$(LIBDIR) -I $(ARCHINCDIR)/ -L$(LIBOUTDIR)/ -fno-builtin -fno-stack-protector -Wall -O2
ASFLAGS = -I $(INCDIR)/ -I $(ARCHINCDIR)/
SERVERCFLAGS	= -I $(INCDIR)/ -I $(SERVERSINCDIR)/ -I$(LIBDIR) -I $(ARCHINCDIR)/ -L$(LIBOUTDIR)/ -Wall -static -O2
MAKEFLAGS	+= --no-print-directory -I $(SRCDIR)/utils/mk/
ARFLAGS	= rcs
MAKE		= make

ifeq ($(CONFIG_DEBUG_INFO),y)
	CFLAGS += -g
	SERVERCFLAGS += -g
endif

export AS ASM CC CPP LD OBJCOPY INSTALL CFLAGS ASFLAGS ARFLAGS HOSTCC HOSTLD SERVERCFLAGS

LYOSKERNEL = $(ARCHDIR)/lyos.elf
ifeq ($(CONFIG_COMPRESS_GZIP),y)
	ZIP = gzip
	LYOSZKERNEL = $(ARCHDIR)/lyos.gz
endif

LYOSINITRD	= $(ARCHDIR)/initrd.tar

DASMOUTPUT	= lyos.elf.asm

COLORDEFAULT= \033[0m
COLORRED	= \033[1;31m
COLORGREEN	= \033[1;32m
COLORYELLOW	= \033[1;33m
COLORBLUE	= \033[1;34m

CONFIGDIR = $(SRCDIR)/include/config
CONFIGIN = $(SRCDIR)/config.in
CONF = $(SRCDIR)/scripts/config/conf
MCONF = $(SRCDIR)/scripts/config/mconf
AUTOCONFINC = $(SRCDIR)/include/config/autoconf.h
CONFIGINC = $(SRCDIR)/.config
DEFCONFIG = $(ARCHDIR)/configs/$(SUBARCH)_defconfig

KCONFIG_AUTOHEADER = include/config/autoconf.h
export KCONFIG_AUTOHEADER

VERBOSE ?= 0
ifeq ($(VERBOSE), 1)
  Q =
else
  Q = @
endif
export Q

# All Phony Targets
.PHONY : all everything disasm clean realclean mrproper install help config menuconfig \
	setup-toolchain libraries install-libraries kernel fs install-fs drivers install-drivers \
	servers install-servers net install-net objdirs kvm kvm-debug install-headers tests

# Default entry point
all : clean everything

include $(ARCHDIR)/Makefile

everything : $(CONFIGINC) $(AUTOCONFINC) genconf objdirs install-headers libraries install-libraries fs install-fs drivers \
		install-drivers servers install-servers net install-net kernel initrd

setup-toolchain:
	@echo -e '$(COLORGREEN)Setting up toolchain...$(COLORDEFAULT)'
	@./scripts/setup-toolchain.sh

$(CONFIGINC):
	@echo -e '$(COLORYELLOW)Using default configuration$(COLORDEFAULT)'
	@cp $(DEFCONFIG) $(CONFIGINC)

$(AUTOCONFINC):
	@$(MAKE) -f Makefile silentoldconfig

genconf:
	@echo -e '$(COLORGREEN)Generating compile.h...$(COLORDEFAULT)'
	@echo -e '\tGEN\tcompile.h'
	@sh ./scripts/genversion.sh
	@$(shell ./scripts/gencompile.sh $(ARCH) $(SUBARCH) $(KERNELVERSION) $(CC) $(CONFIG_LOCALVERSION) $(CONFIG_LOCALVERSION_AUTO) $(CONFIG_SMP))

config: $(CONFIGIN) $(CONFIGINC)
	@(cd scripts/config; make config)
	$(CONF) $(CONFIGIN)
	@$(MAKE) -f Makefile silentoldconfig

menuconfig: $(CONFIGIN) $(CONFIGINC)
	@(cd scripts/config; make menuconfig)
	$(MCONF) $(CONFIGIN)
	@$(MAKE) -f Makefile silentoldconfig

defconfig: $(CONFIGIN) $(CONFIGINC)
	@(cd scripts/config; make config)
	$(CONF) --defconfig=$(DEFCONFIG) $(CONFIGIN)
	@$(MAKE) -f Makefile silentoldconfig

silentoldconfig:
	@rm -rf $(CONFIGDIR)
	@mkdir $(CONFIGDIR)
	@(cd scripts/config; make config)
	$(CONF) --silentoldconfig  $(CONFIGIN)

clean:
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C lib $(MAKEFLAGS) clean
	$(Q)$(MAKE) -C fs $(MAKEFLAGS) clean
	$(Q)$(MAKE) -C drivers $(MAKEFLAGS) clean
	$(Q)$(MAKE) -C servers $(MAKEFLAGS) clean

realclean :
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C lib $(MAKEFLAGS) realclean
	$(Q)$(MAKE) -C fs $(MAKEFLAGS) realclean
	$(Q)$(MAKE) -C drivers $(MAKEFLAGS) realclean
	$(Q)$(MAKE) -C servers $(MAKEFLAGS) realclean
	$(Q)$(MAKE) -C tests $(MAKEFLAGS) realclean
	$(Q)$(MAKE) -C utils $(MAKEFLAGS) realclean

mrproper:
	@echo -e '$(COLORRED)Removing object files...$(COLORDEFAULT)'
	@find . \( -path ./toolchain -o -path ./obj \) -prune -path ./obj -prune -o -name "*.o" -exec rm -f {} \;
	@echo -e '$(COLORRED)Removing compile.h...$(COLORDEFAULT)'
	@rm -f $(INCLUDEDIR)/lyos/compile.h
	@echo -e '$(COLORRED)Removing configure files...$(COLORDEFAULT)'
	@rm -f $(CONFIGINC) $(CONFIGINC).old
	@rm -rf $(CONFIGDIR)

install: install-libraries install-fs install-drivers

update-disk:
	$(Q)$(MAKE) -C utils $(MAKEFLAGS)
	$(Q)$(MAKE) -C utils $(MAKEFLAGS) install
	@sudo bash scripts/update-disk.sh

kvm:
	@qemu-system-x86_64 -smp 2 -kernel $(LYOSKERNEL) -append "console=ttyS0 video=1024x768 watchdog=1" -initrd "$(LYOSINITRD),$(DESTDIR)/sbin/mm,$(DESTDIR)/sbin/pm,$(DESTDIR)/sbin/servman,$(DESTDIR)/sbin/devman,$(DESTDIR)/sbin/sched,$(DESTDIR)/sbin/vfs,$(DESTDIR)/sbin/systask,$(DESTDIR)/sbin/tty,$(DESTDIR)/sbin/ramdisk,$(DESTDIR)/sbin/initfs,$(DESTDIR)/sbin/sysfs,$(DESTDIR)/sbin/ipc,$(DESTDIR)/sbin/netlink,$(DESTDIR)/sbin/pci,$(DESTDIR)/sbin/init" -net nic,model=rtl8139 -net user -drive id=hda,file=lyos-disk.img,format=raw,if=none -device virtio-blk-pci,drive=hda -m 2048 -serial stdio -vga virtio -sdl -cpu host,pmu=true --enable-kvm

kvm-telnet:
	@qemu-system-x86_64 -smp 2 -kernel $(LYOSKERNEL) -append "console=ttyS0 video=1024x768 watchdog=1" -initrd "$(LYOSINITRD),$(DESTDIR)/sbin/mm,$(DESTDIR)/sbin/pm,$(DESTDIR)/sbin/servman,$(DESTDIR)/sbin/devman,$(DESTDIR)/sbin/sched,$(DESTDIR)/sbin/vfs,$(DESTDIR)/sbin/systask,$(DESTDIR)/sbin/tty,$(DESTDIR)/sbin/ramdisk,$(DESTDIR)/sbin/initfs,$(DESTDIR)/sbin/sysfs,$(DESTDIR)/sbin/ipc,$(DESTDIR)/sbin/pci,$(DESTDIR)/sbin/init" -net nic,model=rtl8139 -net user -hda lyos-disk.img -m 2048 -serial telnet:localhost:4321,server,nowait -nographic -cpu host,pmu=true --enable-kvm

kvm-disk:
	@qemu-system-x86_64 -smp 2 -drive id=hda,file=lyos-disk.img,format=raw,if=none -device virtio-blk-pci,drive=hda -m 2048 -serial stdio -vga std -sdl -cpu host,pmu=true --enable-kvm

kvm-debug:
	@qemu-system-i386 -s -S -smp 2 -kernel $(LYOSKERNEL) -append "console=ttyS0 video=1024x768 watchdog=1" -initrd "$(LYOSINITRD),$(DESTDIR)/sbin/mm,$(DESTDIR)/sbin/pm,$(DESTDIR)/sbin/servman,$(DESTDIR)/sbin/devman,$(DESTDIR)/sbin/sched,$(DESTDIR)/sbin/vfs,$(DESTDIR)/sbin/systask,$(DESTDIR)/sbin/tty,$(DESTDIR)/sbin/ramdisk,$(DESTDIR)/sbin/initfs,$(DESTDIR)/sbin/sysfs,$(DESTDIR)/sbin/ipc,$(DESTDIR)/sbin/pci,$(DESTDIR)/sbin/init" -net nic,model=rtl8139 -net user -drive id=hda,file=lyos-disk.img,format=raw,if=none -device virtio-blk-pci,drive=hda -m 1024 -serial stdio -vga virtio -sdl -cpu host --enable-kvm

disk-image:
	$(Q)$(MAKE) -C utils $(MAKEFLAGS)
	$(Q)$(MAKE) -C utils $(MAKEFLAGS) install
	@sudo bash scripts/setup-disk.sh

initrd:
	$(Q)$(MAKE) -C utils $(MAKEFLAGS)
	$(Q)$(MAKE) -C utils $(MAKEFLAGS) install
	@(cd ramdisk; make)

disasm :
	@echo -e '$(COLORBLUE)Disassembling the kernel...$(COLORDEFAULT)'
	@echo -e '\tDASM\t$(LYOSKERNEL)'
	@$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

help :
	@echo "Make options:"
	@echo "-----------------------------------------------------------------"
	@echo "make\t\t: build everything."
	@echo "-----------------------------------------------------------------"
	@echo "make clean\t: remove all object files but keep config files."
	@echo "make realclean\t: remove all object files and config file."
	@echo "make install\t: install everything to DESTDIR."

objdirs:
	@(mkdir -p $(OBJDIR))
	@(mkdir -p $(DESTDIR))
	@(mkdir -p $(DESTDIR)/bin)
	@(mkdir -p $(DESTDIR)/boot)
	@(mkdir -p $(DESTDIR)/lib)
	@(mkdir -p $(DESTDIR)/sbin)
	@(mkdir -p $(DESTDIR)/usr/include)
	@(mkdir -p $(DESTDIR)/usr/bin)
	@(mkdir -p $(DESTDIR)/usr/sbin)

libraries:
	@echo -e '$(COLORGREEN)Compiling libraries...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C lib $(MAKEFLAGS)

install-libraries:
	@echo -e '$(COLORGREEN)Installing libraries...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C lib $(MAKEFLAGS) install

install-headers:
	@echo -e '$(COLORGREEN)Installing headers...$(COLORDEFAULT)'
	@cp -rf include/uapi/* $(DESTDIR)/usr/include
	@cp -rf $(ARCHINCDIR)/uapi/asm $(DESTDIR)/usr/include

kernel:
	@echo -e '$(COLORGREEN)Compiling the kernel...$(COLORDEFAULT)'
	@(cd kernel; make)
ifeq ($(CONFIG_COMPRESS_GZIP),y)
	@@echo -e '$(COLORGREEN)Compressing the kernel...$(COLORDEFAULT)'
	@echo -e '\tZIP\t$(LYOSZKERNEL)'
	@$(ZIP) -cfq $(LYOSKERNEL) > $(LYOSZKERNEL)
	@cp -f $(LYOSZKERNEL) $(DESTDIR)/boot/
endif
	@@echo -e '$(COLORGREEN)Kernel is ready.$(COLORDEFAULT)'

fs:
	@echo -e '$(COLORGREEN)Compiling filesystem servers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C fs $(MAKEFLAGS)

install-fs:
	@echo -e '$(COLORGREEN)Installing filesystem servers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C fs $(MAKEFLAGS) install

drivers:
	@echo -e '$(COLORGREEN)Compiling device drivers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C drivers $(MAKEFLAGS)

install-drivers:
	@echo -e '$(COLORGREEN)Installing device drivers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C drivers $(MAKEFLAGS) install

servers:
	@echo -e '$(COLORGREEN)Compiling servers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C servers $(MAKEFLAGS)

install-servers:
	@echo -e '$(COLORGREEN)Installing servers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C servers $(MAKEFLAGS) install

net:
	@echo -e '$(COLORGREEN)Compiling network drivers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C net $(MAKEFLAGS)

install-net:
	@echo -e '$(COLORGREEN)Installing network drivers...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C net $(MAKEFLAGS) install

tests:
	@echo -e '$(COLORGREEN)Compiling and installing tests...$(COLORDEFAULT)'
	$(Q)$(MAKE) -C tests $(MAKEFLAGS)
	$(Q)$(MAKE) -C tests $(MAKEFLAGS) install
