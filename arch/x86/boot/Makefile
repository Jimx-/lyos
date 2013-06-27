ASM		= nasm
ASMBFLAGS	= -I include/
LYOSBOOT	= boot.bin hdboot.bin loader.bin hdloader.bin newboot.bin newloader.bin

everything : realclean $(LYOSBOOT)

realclean :
	@rm -f $(LYOSBOOT)

genconf:
	@echo -e '\tGEN\tcompile.h'
	@$(shell ./gencompinc.sh $(ARCH) $(KERNELVERSION) $(CC))


%.bin: %.asm
	@echo -e '\tASM\tarch/x86/boot/$@'
	@$(ASM) $(ASMBFLAGS) -o $@ $<	


