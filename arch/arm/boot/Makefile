
CC	= arm-elf-gcc
LD	= arm-elf-ld
OBJCOPY	= arm-elf-objcopy

CFLAGS	= -O2 -g
ASFLAGS	= -O2 -g
LDFLAGS	= -Tboot.lds -Ttext 30000000 

OBJS	= init.o start.o boot.o vector.o

.c.o:
	$(CC) $(CFLAGS) -c $<
.s.o:
	$(CC) $(ASFLAGS) -c $<

.PHONY: everything clean

everything: clean boot

boot:	$(OBJS)
	$(CC) -static -nostartfiles -nostdlib $(LDFLAGS) $? -o $@ -lgcc
	$(OBJCOPY) -O binary $@ boot.bin

clean:
	rm *.o boot boot.bin -f
