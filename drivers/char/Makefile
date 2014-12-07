#
# Makefile for the Lyos character device drivers

TTY = tty/tty

everything : realclean $(TTY)

realclean :
	@rm -f $(TTY)

$(TTY):
	@(cd tty; make)
	@(cp $(TTY) $(DESTDIR)/sbin)
