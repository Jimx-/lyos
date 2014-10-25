#
# Makefile for the Lyos tty driver.

TTYOBJS 		= arch/$(ARCH)/arch-tty.o \
			_tty.o global.o
TTY		= tty.o

CFLAGS += -I$(CURDIR)

everything : realclean $(TTY)

realclean :
	@rm -f $(TTYOBJS)

$(TTY): $(TTYOBJS)
	@echo -e '\tLD\tdrivers/char/tty/$@'
	@$(LD) -r -o $(TTY) $(TTYOBJS)

arch/$(ARCH)/arch-tty.o:
	@(cd arch/$(ARCH)/; make)

_tty.o: tty.c
	@echo -e '\tCC\tdrivers/char/tty/$@'
	@$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tdrivers/char/tty/$@'
	@$(CC) $(CFLAGS) -o $@ $<

