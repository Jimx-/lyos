
SYSCMDS = $(DESTDIR)/bin/service $(DESTDIR)/bin/mount

everything : realclean $(SYSCMDS)

all : realclean $(SYSCMDS)

realclean :
	@rm -f $(SYSCMDS)

$(DESTDIR)/bin/service:
	@(cd service; make)

$(DESTDIR)/bin/mount: mount.c
	@$(CC) -o $@ $<
