#
# Makefile for the Lyos tty driver.

TTYOBJS 		= arch/$(ARCH)/arch-tty.o \
			tty.o global.o
TTY		= tty
LIBS	= -llyos -ldriver

CFLAGS += -I$(CURDIR)
SERVERCFLAGS += -I$(CURDIR)

everything : realclean $(TTY)

realclean :
	@rm -f $(TTYOBJS) $(TTY)

$(TTY): $(TTYOBJS)
	@echo -e '\tLD\tdrivers/char/tty/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) -Xlinker -Ttext -Xlinker 0x50000000

arch/$(ARCH)/arch-tty.o:
	@(cd arch/$(ARCH)/; make)

%.o: %.c
	@echo -e '\tCC\tdrivers/char/tty/$@'
	@$(CC) $(SERVERCFLAGS) -c $<

