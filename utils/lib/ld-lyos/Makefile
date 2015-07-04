
LDLYOS			= $(DESTDIR)/lib/ld-lyos.so
LDLYOSOBJS 		= main.o arch/$(ARCH)/arch-ldso.o

CFLAGS			= -I$(SRCDIR)/include/lyos

everything : realclean $(LDLYOS)

realclean :
	@rm -f $(LDLYOS) $(LDLYOSOBJS)

$(LDLYOS): $(LDLYOSOBJS)
	@$(CC) -shared -nostartfiles -nodefaultlibs -o $@ $^

%.o: %.c
	@$(CC) -c $<

arch/$(ARCH)/arch-ldso.o:
	@(cd arch/$(ARCH)/; make)
