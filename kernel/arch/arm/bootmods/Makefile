####################################
# Makefile for Lyos boot modules.  #
####################################

BOOTMODSOBJS	= mm.o pm.o servman.o devman.o sched.o vfs.o systask.o tty.o ramdisk.o \
					initfs.o sysfs.o init.o

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

servman.o: $(DESTDIR)/sbin/servman
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm servman $@
	@rm ./servman


devman.o: $(DESTDIR)/sbin/devman
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm devman $@
	@rm ./devman

sched.o: $(DESTDIR)/sbin/sched
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm sched $@
	@rm ./sched

vfs.o: $(DESTDIR)/sbin/vfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm vfs $@
	@rm ./vfs

systask.o: $(DESTDIR)/sbin/systask
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm systask $@
	@rm ./systask

tty.o: $(DESTDIR)/sbin/tty
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm tty $@
	@rm ./tty

ramdisk.o: $(DESTDIR)/sbin/ramdisk
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm ramdisk $@
	@rm ./ramdisk

initfs.o: $(DESTDIR)/sbin/initfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm initfs $@
	@rm ./initfs

sysfs.o: $(DESTDIR)/sbin/sysfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm sysfs $@
	@rm ./sysfs

init.o: $(DESTDIR)/sbin/init
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm init $@
	@rm ./init
