#
# Makefile for the Lyos character device drivers

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

CHRDRVOBJS	= console.o keyboard.o tty.o
CHRDRV		= chr_dev.o

everything : realclean $(CHRDRV)

realclean :
	@rm -f $(CHRDRVOBJS)

$(CHRDRV): $(CHRDRVOBJS)
	@echo -e '\tLD\tdrivers/chr_dev/$@'
	@$(LD) -r -o $(CHRDRV) $(CHRDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/chr_dev/$@'
	@$(CC) $(CFLAGS) -o $@ $<

