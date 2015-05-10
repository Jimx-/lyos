#
# Makefile for keyboard driver.

KB		= keyboard
KBOBJS = keyboard.o

LIBS	= -llyos -linputdriver -lsysfs

everything : realclean $(KB)

realclean :
	@rm -f $(KB) $(KBOBJS)

$(KB): $(KBOBJS)
	@echo -e '\tCC\tdrivers/hid/keyboard/$@'
	@$(CC) $(SERVERCFLAGS) -o $@ $^ $(LIBS) -Xlinker -Ttext -Xlinker 0x50000000

%.o: %.c
	@echo -e '\tCC\tdrivers/hid/keyboard/$@'
	@$(CC) $(SERVERCFLAGS) -c $<
