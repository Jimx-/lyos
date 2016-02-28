
LDLYOS			= ld-lyos.so
LDLYOSOBJS 		= arch/$(ARCH)/arch-ldso.o ldso.o env.o xprintf.o elf.o paths.o load.o map.o reloc.o \
					symbol.o

CFLAGS			= -I$(SRCDIR)/include/ -I$(SRCDIR)/include/lyos/ -I$(CURDIR) -g

.PHONY : everything realclean install

everything : realclean $(LDLYOS)

realclean :
	@rm -f $(LDLYOS) $(LDLYOSOBJS)

install :
	$(Q)$(INSTALL) $(LDLYOS) $(DESTDIR)/lib

$(LDLYOS): $(LDLYOSOBJS)
	@$(CC) -Wl,-Bsymbolic -shared -nostartfiles -nodefaultlibs -o $@ $^

%.o: %.c
	@$(CC) -c -g $<

arch/$(ARCH)/arch-ldso.o:
	@(cd arch/$(ARCH)/; make)
