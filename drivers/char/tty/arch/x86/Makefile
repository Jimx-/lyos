
ARCHTTYOBJS		= console.o keyboard.o

ARCHTTY		= arch-tty.o
LIBS		= -llyos

everything : realclean $(ARCHTTY)

realclean :
	@rm -f $(ARCHTTYOBJS)

$(ARCHTTY): $(ARCHTTYOBJS)
	@echo -e '\tLD\tdrivers/char/tty/arch/x86/$@'
	@$(LD) -r -o $(ARCHTTY) $(ARCHTTYOBJS)

%.o: %.c
	@echo -e '\tCC\tdrivers/char/tty/arch/x86/$@'
	@$(CC) $(CFLAGS) -o $@ $<
