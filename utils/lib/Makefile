
LIBS = $(DESTDIR)/lib/ld-lyos.so

everything : realclean $(LIBS)

all : realclean $(LIBS)

realclean :
	@rm -f $(LIBS)

$(DESTDIR)/lib/ld-lyos.so:
	@(cd ld-lyos; make)
