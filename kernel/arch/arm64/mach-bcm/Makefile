obj-y	= uart-aux.o

obj-$(CONFIG_ARCH_BCM2835) += bcm2835.o bcm2835-irq.o bcm2836-irq.o

BCM = mach-bcm.o

everything : realclean $(BCM)

realclean :
	@rm -f $(BCM) $(obj-y)

$(BCM): $(obj-y)
	@echo -e '\tLD\tkernel/arch/arm64/mach-bcm/$@'
	@$(LD) -r -o $(BCM) $(obj-y)

%.o: %.c
	@echo -e '\tCC\tkernel/arch/arm64/mach-bcm/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
