ASM		= nasm
ASMBFLAGS	= -I include/
LYOSBOOT	= boot.bin hdboot.bin loader.bin hdloader.bin

everything : reclean $(LYOSBOOT)

reclean :
	@rm -f $(LYOSBOOT)

%.bin: %.asm
	@echo -e '\tASM\tarch/x86/boot/$@'
	@$(ASM) $(ASMBFLAGS) -o $@ $<	

