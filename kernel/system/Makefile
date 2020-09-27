##############################
# Makefile for Lyos kernel.  #
##############################

SYSOBJS	= sys_printx.o sys_datacopy.o sys_getinfo.o sys_privctl.o sys_vmctl.o sys_umap.o \
			sys_portio.o sys_vportio.o sys_irqctl.o sys_fork.o sys_clear.o sys_exec.o \
			sys_sigsend.o sys_sigreturn.o sys_kill.o sys_getksig.o sys_endksig.o \
			sys_times.o sys_trace.o sys_alarm.o sys_kprofile.o sys_setgrant.o \
			sys_safecopy.o

SYSTEM		= system.o

everything : realclean $(SYSTEM)

realclean :
	@rm -f $(SYSOBJS)

$(SYSTEM): $(SYSOBJS)
	@echo -e '\tLD\tkernel/system/$@'
	@$(LD) -r -o $(SYSTEM) $(SYSOBJS)

%.o: %.c
	@echo -e '\tCC\tkernel/system/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
