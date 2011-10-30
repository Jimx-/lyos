#########################
# Makefile for Lyos     #
#########################

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
LYOSBOOT	= boot/boot.bin boot/hdboot.bin boot/loader.bin boot/hdloader.bin
LYOSKERNEL	= kernel.bin
LIB		= lib/Lyoscrt.a

OBJS		= kernel/kernel.o kernel/start.o kernel/main.o\
			kernel/clock.o \
			kernel/i8259.o kernel/global.o kernel/protect.o kernel/proc.o\
			kernel/systask.o\
			kernel/kliba.o kernel/klib.o kernel/sys.o kernel/reboot.o\
			lib/syslog.o\
			mm/mm.o \
			fs/fs.o \
			drivers/drivers.o
LOBJS		=  lib/syscall.o\
			lib/printf.o lib/vsprintf.o\
			lib/string.o lib/misc.o\
			lib/open.o lib/read.o lib/write.o lib/close.o lib/unlink.o\
			lib/lseek.o\
			lib/getpid.o lib/getuid.o lib/stat.o\
			lib/fork.o lib/exit.o lib/wait.o lib/exec.o lib/kill.o \
			lib/uname.o
DASMOUTPUT	= kernel.bin.asm

# All Phony Targets
.PHONY : everything final image clean realclean disasm all buildimg

# Default starting position
nop :
	@echo "make image : build the kernel and the boot image."
	@echo "make cmd   : install the command files."

everything : $(LYOSBOOT) $(LYOSKERNEL)

all : realclean everything image  lib cmd

image : realclean everything clean buildimg

cmd :
	(cd command;make install)

lib : 
	(cd command;make install)

clean :
	rm -f $(OBJS) $(LOBJS)

realclean :
	rm -f $(OBJS) $(LOBJS) $(LIB) $(LYOSBOOT) $(LYOSKERNEL)

disasm :
	$(DASM) $(DASMFLAGS) $(LYOSKERNEL) > $(DASMOUTPUT)

buildimg :
	dd if=boot/boot.bin of=$(FD) bs=512 count=1 conv=notrunc
	dd if=boot/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' boot/include/load.inc | sed -e 's/.*0x//g'\`*200" | bc` bs=1 count=446 conv=notrunc
	dd if=boot/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' boot/include/load.inc | sed -e 's/.*0x//g'\`*200+1FE" | bc` skip=510 bs=1 count=2 conv=notrunc
	sudo mount -o loop $(FD) /mnt/floppy/
	sudo cp -fv boot/loader.bin /mnt/floppy/
	sudo cp -fv kernel.bin /mnt/floppy
	sudo umount /mnt/floppy

boot/boot.bin : boot/boot.asm boot/include/load.inc boot/include/fat12hdr.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/hdboot.bin : boot/hdboot.asm boot/include/load.inc boot/include/fat12hdr.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/loader.bin : boot/loader.asm boot/include/load.inc boot/include/fat12hdr.inc boot/include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/hdloader.bin : boot/hdloader.asm boot/include/load.inc boot/include/fat12hdr.inc boot/include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

$(LYOSKERNEL) : $(OBJS) $(LIB)
	$(LD) $(LDFLAGS) -o $(LYOSKERNEL) $^

$(LIB) : $(LOBJS)
	$(AR) $(ARFLAGS) $@ $^

kernel/kernel.o : kernel/kernel.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

lib/syscall.o : lib/syscall.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

kernel/reboot.o : kernel/reboot.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

kernel/start.o: kernel/start.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/main.o: kernel/main.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/clock.o: kernel/clock.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/i8259.o: kernel/i8259.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/global.o: kernel/global.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/protect.o: kernel/protect.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/proc.o: kernel/proc.c
	$(CC) $(CFLAGS) -o $@ $<

lib/printf.o: lib/printf.c
	$(CC) $(CFLAGS) -o $@ $<

lib/vsprintf.o: lib/vsprintf.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/systask.o: kernel/systask.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/klib.o: kernel/klib.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/sys.o: kernel/sys.c
	$(CC) $(CFLAGS) -o $@ $<

lib/misc.o: lib/misc.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/kliba.o : kernel/kliba.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

lib/string.o : lib/string.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

lib/open.o: lib/open.c
	$(CC) $(CFLAGS) -o $@ $<

lib/read.o: lib/read.c
	$(CC) $(CFLAGS) -o $@ $<

lib/write.o: lib/write.c
	$(CC) $(CFLAGS) -o $@ $<

lib/close.o: lib/close.c
	$(CC) $(CFLAGS) -o $@ $<

lib/unlink.o: lib/unlink.c
	$(CC) $(CFLAGS) -o $@ $<

lib/getpid.o: lib/getpid.c
	$(CC) $(CFLAGS) -o $@ $<
	
lib/getuid.o: lib/getuid.c
	$(CC) $(CFLAGS) -o $@ $<

lib/syslog.o: lib/syslog.c
	$(CC) $(CFLAGS) -o $@ $<

lib/fork.o: lib/fork.c
	$(CC) $(CFLAGS) -o $@ $<

lib/exit.o: lib/exit.c
	$(CC) $(CFLAGS) -o $@ $<

lib/wait.o: lib/wait.c
	$(CC) $(CFLAGS) -o $@ $<

lib/exec.o: lib/exec.c
	$(CC) $(CFLAGS) -o $@ $<

lib/stat.o: lib/stat.c
	$(CC) $(CFLAGS) -o $@ $<

lib/lseek.o: lib/lseek.c
	$(CC) $(CFLAGS) -o $@ $<

lib/kill.o: lib/kill.c
	$(CC) $(CFLAGS) -o $@ $<

lib/uname.o: lib/uname.c
	$(CC) $(CFLAGS) -o $@ $<

mm/mm.o:
	(cd mm; make)

fs/fs.o:
	(cd fs; make)

drivers/drivers.o:
	(cd drivers; make)
