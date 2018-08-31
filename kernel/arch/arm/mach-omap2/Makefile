obj-y += common.o interrupt.o
obj-$(CONFIG_MACH_OMAP3_BEAGLE) += omap3_beagle.o
obj-$(CONFIG_MACH_TI_AM335X_EVM) += am335xevm.o

OMAP2 = mach-omap2.o

everything : realclean $(OMAP2)

realclean :
	@rm -f $(OMAP2) $(obj-y)

$(OMAP2): $(obj-y)
	@echo -e '\tLD\tkernel/arch/arm/mach-omap2/$@'
	@$(LD) -r -o $(OMAP2) $(obj-y)

%.o: %.c
	@echo -e '\tCC\tkernel/arch/arm/mach-omap2/$@'
	@$(CC) $(CFLAGS) -c -o $@ $<
