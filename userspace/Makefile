

COMMANDS 	= ../sysroot/usr/bin/getty ../sysroot/usr/bin/login

everything : realclean $(COMMANDS)

all : realclean $(COMMANDS)

realclean :
	@rm -f $(COMMANDS)

../sysroot/usr/bin/getty : getty/getty.c
	@$(CC) -o $@ $<

../sysroot/usr/bin/login : login/login.c
	@$(CC) -o $@ $<
