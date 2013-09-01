#
# Makefile for the Lyos character device drivers

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

CHRDRVOBJS	= console.o keyboard.o tty.o
CHRDRV		= char.o

everything : realclean $(CHRDRV)

realclean :
	@rm -f $(CHRDRVOBJS)

$(CHRDRV): $(CHRDRVOBJS)
	@echo -e '\tLD\tdrivers/char/$@'
	@$(LD) -r -o $(CHRDRV) $(CHRDRVOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/char/$@'
	@$(CC) $(CFLAGS) -o $@ $<

