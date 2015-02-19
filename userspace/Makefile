
COMMANDS 	= $(DESTDIR)/usr/bin/getty $(DESTDIR)/usr/bin/login \

everything : realclean $(COMMANDS)

all : realclean $(COMMANDS)

realclean :
	@rm -f $(COMMANDS)

$(DESTDIR)/usr/bin/getty : getty/getty.c
	@$(CC) -o $@ $<

$(DESTDIR)/usr/bin/login : login/login.c
	@$(CC) -o $@ $<
