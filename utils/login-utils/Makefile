
LOGINCMDS = $(DESTDIR)/usr/bin/getty $(DESTDIR)/usr/bin/login

everything : realclean $(LOGINCMDS)

all : realclean $(LOGINCMDS)

realclean :
	@rm -f $(LOGINCMDS)

$(DESTDIR)/usr/bin/getty: getty.c
	@$(CC) -o $@ $< 

$(DESTDIR)/usr/bin/login: login.c
	@$(CC) -o $@ $<
