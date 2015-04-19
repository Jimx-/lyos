
CMDS = $(DESTDIR)/bin/cat

everything : realclean $(CMDS)

all : realclean $(CMDS)

realclean :
	@rm -f $(CMDS)

$(DESTDIR)/bin/cat: cat.c
	@$(CC) -o $@ $<
