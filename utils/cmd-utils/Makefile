
CMDS = $(DESTDIR)/bin/cat $(DESTDIR)/bin/ls

everything : realclean $(CMDS)

all : realclean $(CMDS)

realclean :
	@rm -f $(CMDS)

$(DESTDIR)/bin/cat: cat.c
	@$(CC) -o $@ $<

$(DESTDIR)/bin/ls:
	@(cd ls; make)
