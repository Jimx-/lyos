

CFLAGS = -I $(INCDIR)/ -I$(LIBDIR) -I $(ARCHINCDIR)/ -L$(LIBOUTDIR)/ -Wall -static
INSTALL_PREFIX = /sbin

include lyos.prog.mk
