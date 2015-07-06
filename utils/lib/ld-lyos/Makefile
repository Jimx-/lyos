
LDLYOS			= $(DESTDIR)/lib/ld-lyos.so
LDLYOSOBJS 		= arch/$(ARCH)/arch-ldso.o ldso.o env.o xprintf.o elf.o paths.o load.o

CFLAGS			= -I$(SRCDIR)/include/ -I$(SRCDIR)/include/lyos/ -I$(PWD) -g

everything : realclean $(LDLYOS)

realclean :
	@rm -f $(LDLYOS) $(LDLYOSOBJS)

$(LDLYOS): $(LDLYOSOBJS)
	@$(CC) -Wl,-Bsymbolic -shared -nostartfiles -nodefaultlibs -o $@ $^

%.o: %.c
	@$(CC) -c -g $<

arch/$(ARCH)/arch-ldso.o:
	@(cd arch/$(ARCH)/; make)
