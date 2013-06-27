
CC	= arm-elf-gcc
LD	= arm-elf-ld

CFLAGS	= -O2 -g
ASMFLAGS	= -O2 -g
LDFLAGS	= -Tboot/boot.lds -Ttext 30000000 
MAKEFLAGS = --no-print-directory

OBJS	= boot/arm-boot.o kernel/arm-krnl.o

KERNEL	= lyos

.PHONY: everything clean

everything: clean $(KERNEL)

$(KERNEL): $(OBJS)
	@echo -e '\tCC\t$@'
	@$(CC) -static -nostartfiles -nostdlib $(LDFLAGS) $? -o $@ -lgcc

boot/arm-boot.o:
	@(cd boot; make)

kernel/arm-krnl.o:
	@(cd kernel; make)

run:
	qemu-system-arm -M versatilepb -m 1024 -kernel $(KERNEL) -serial stdio

clean:
	@find -name *.o -exec rm -f {} \;
