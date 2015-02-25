
SYSCMDS = $(DESTDIR)/bin/service

everything : realclean $(SYSCMDS)

all : realclean $(SYSCMDS)

realclean :
	@rm -f $(SYSCMDS)

$(DESTDIR)/bin/service:
	@(cd service; make)
