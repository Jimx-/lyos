LDSOOBJS		= _start.o gate_intr.o init.o syscalls.o memset.o memcpy.o strlen.o \
					arch_reloc.o crti.o crtn.o set_thread_area.o tls.o

ARCHLDSO		= arch-ldso.o

everything : realclean $(ARCHLDSO)

realclean :
	@rm -f $(LDSOOBJS)

$(ARCHLDSO): $(LDSOOBJS)
	@$(LD) -r -o $(ARCHLDSO) $(LDSOOBJS)

%.o: %.S
	@$(AS) -o $@ $<

%.o: %.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

gate_intr.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/i386/gate_intr.S
	@$(AS) -o $@ $<

crti.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/i386/crti.S
	@$(AS) -o $@ $<

crtn.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/i386/crtn.S
	@$(AS) -o $@ $<

init.o:	$(SRCDIR)/toolchain/patches/newlib/lyos/init.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

syscalls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/syscalls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

set_thread_area.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/i386/set_thread_area.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

tls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/i386/tls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<
