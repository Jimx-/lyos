ASM			= nasm
ASMBFLAGS	= -I include/
LYOSBOOT	= boot.bin hdboot.bin loader.bin hdloader.bin

everything : reclean $(LYOSBOOT)

reclean :
	rm -f $(LYOSBOOT)

boot.bin : boot.asm include/load.inc include/fat12hdr.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

hdboot.bin : hdboot.asm include/load.inc include/fat12hdr.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

loader.bin : loader.asm include/load.inc include/fat12hdr.inc include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

hdloader.bin : hdloader.asm include/load.inc include/fat12hdr.inc include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<
