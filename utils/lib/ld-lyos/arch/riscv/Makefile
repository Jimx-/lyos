LDSOOBJS		= _start.o gate_intr.o init.o syscalls.o crti.o crtn.o arch_reloc.o memcpy.o memset.o \
					strlen.o tls.o

ARCHLDSO		= arch-ldso.o

everything : realclean $(ARCHLDSO)

realclean :
	@rm -f $(LDSOOBJS)

$(ARCHLDSO): $(LDSOOBJS)
	@$(LD) -r -o $(ARCHLDSO) $(LDSOOBJS)

%.o: %.S
	@$(CC) $(CFLAGS) -fPIC -c -D__ASSEMBLY__ -o $@ $<

%.o: %.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

gate_intr.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/gate_intr.S
	@$(AS) -o $@ $<

crti.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/crti.S
	@$(AS) -o $@ $<

crtn.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/crtn.S
	@$(AS) -o $@ $<

init.o:	$(SRCDIR)/toolchain/patches/newlib/lyos/init.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

syscalls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/syscalls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

tls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/tls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<
