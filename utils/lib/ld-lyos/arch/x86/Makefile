OBJS-$(CONFIG_X86_32) = _start.o arch_reloc.o memset.o memcpy.o

OBJS-$(CONFIG_X86_64) = _start_64.o arch_reloc_64.o memset_64.o memcpy_64.o

LDSOOBJS		= $(OBJS-y) gate_intr.o init.o syscalls.o strlen.o \
					crti.o crtn.o set_thread_area.o tls.o

ARCHLDSO		= arch-ldso.o

ifeq ($(SUBARCH),x86_64)
	MACHINE = x86_64
else
	MACHINE = i386
endif

everything : realclean $(ARCHLDSO)

realclean :
	@rm -f $(LDSOOBJS)

$(ARCHLDSO): $(LDSOOBJS)
	@$(LD) -r -o $(ARCHLDSO) $(LDSOOBJS)

%.o: %.S
	@$(AS) -o $@ $<

%.o: %.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

gate_intr.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/$(MACHINE)/gate_intr.S
	@$(AS) -o $@ $<

crti.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/$(MACHINE)/crti.S
	@$(AS) -o $@ $<

crtn.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/$(MACHINE)/crtn.S
	@$(AS) -o $@ $<

init.o:	$(SRCDIR)/toolchain/patches/newlib/lyos/init.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

syscalls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/syscalls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

set_thread_area.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/$(MACHINE)/set_thread_area.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<

tls.o: $(SRCDIR)/toolchain/patches/newlib/lyos/machine/$(MACHINE)/tls.c
	@$(CC) $(CFLAGS) -fPIC -c -o $@ $<
