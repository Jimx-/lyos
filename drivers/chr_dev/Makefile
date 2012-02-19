#
# Makefile for the Lyos character device drivers

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf
CFLAGS		= -c -fno-builtin -fno-stack-protector -fpack-struct -Wall

CHRDRVOBJS	= console.o keyboard.o tty.o
CHRDRV		= chr_dev.o

everything : reclean $(CHRDRV)

reclean :
	@rm -f $(CHRDRVOBJS)

$(CHRDRV): $(CHRDRVOBJS)
	@echo -e '\tLD\tdrivers/chr_dev/$@'
	@$(LD) -r -o $(CHRDRV) $(CHRDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/chr_dev/$@'
	@$(CC) $(CFLAGS) -o $@ $<

