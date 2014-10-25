#
# Makefile for the Lyos character device drivers

TTY = tty/tty.o

CHRDRVOBJS	= $(TTY)
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

$(TTY):
	@(cd tty; make)
