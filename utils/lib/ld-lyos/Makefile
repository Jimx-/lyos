LDLYOS			= ld-lyos.so
LDLYOSOBJS		= arch/$(ARCH)/arch-ldso.o ldso.o env.o xprintf.o elf.o paths.o load.o map.o \
					reloc.o symbol.o tls.o debug.o

CFLAGS = -I$(SRCDIR)/include/ -I$(CURDIR) -g
CFLAGS += -fPIC -fno-builtin -fno-stack-protector -fvisibility=hidden \
		  -ffunction-sections -fdata-sections
export CFLAGS

.PHONY : everything realclean install

everything : realclean $(LDLYOS)

realclean :
	@rm -f $(LDLYOS) $(LDLYOSOBJS)

install :
	$(Q)$(INSTALL) $(LDLYOS) $(DESTDIR)/lib

$(LDLYOS): $(LDLYOSOBJS)
	@$(CC) -Wl,-Bsymbolic,--gc-sections,-z,defs -shared -nostartfiles -nodefaultlibs -o $@ $^

%.o: %.c
	@$(CC) $(CFLAGS) -c -o $@ $<

arch/$(ARCH)/arch-ldso.o:
	@(cd arch/$(ARCH)/; make)
