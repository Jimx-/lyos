
CMDS = $(DESTDIR)/bin/cat $(DESTDIR)/usr/bin/strace

everything : realclean $(CMDS)

all : realclean $(CMDS)

realclean :
	@rm -f $(CMDS)

$(DESTDIR)/bin/cat: cat.c
	@$(CC) -o $@ $<

$(DESTDIR)/usr/bin/strace: strace.c
	@$(CC) -I$(INCDIR) -o $@ $<
