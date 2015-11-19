####################################
# Makefile for Lyos boot modules.  #
####################################

BOOTMODSOBJS	= mm.o pm.o

BOOTMODS		= bootmods.o

everything : realclean $(BOOTMODS)

realclean :
	@rm -f $(BOOTMODS) $(BOOTMODSOBJS)

$(BOOTMODS): $(BOOTMODSOBJS)
	@echo -e '\tLD\tkernel/bootmods/$@'
	@$(LD) -r -o $(BOOTMODS) $^

mm.o: $(DESTDIR)/sbin/mm
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm mm $@
	@rm ./mm

pm.o: $(DESTDIR)/sbin/pm
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm pm $@
	@rm ./pm
