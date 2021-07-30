LDSOOBJS		= init.o syscalls.o crti.o crtn.o

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

crti.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/crti.S
	@$(AS) -o $@ $<

crtn.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/riscv/crtn.S
	@$(AS) -o $@ $<

init.o:	$(SRCDIR)/toolchain/patches/newlib/lyos/init.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

syscalls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/syscalls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<
