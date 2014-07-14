

COMMANDS 	= ../sysroot/usr/bin/getty

everything : realclean $(COMMANDS)

all : realclean $(COMMANDS)

realclean :
	@rm -f $(COMMANDS)

../sysroot/usr/bin/getty : getty/getty.c
	@$(CC) -o $@ $<
