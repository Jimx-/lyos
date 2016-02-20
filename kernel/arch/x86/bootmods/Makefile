####################################
# Makefile for Lyos boot modules.  #
####################################

BOOTMODSOBJS	= mm.o pm.o servman.o devman.o sched.o vfs.o systask.o tty.o ramdisk.o \
					initfs.o sysfs.o ipc.o pci.o init.o

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
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 mm $@
	@rm ./mm

pm.o: $(DESTDIR)/sbin/pm
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 pm $@
	@rm ./pm

servman.o: $(DESTDIR)/sbin/servman
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 servman $@
	@rm ./servman

devman.o: $(DESTDIR)/sbin/devman
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 devman $@
	@rm ./devman

sched.o: $(DESTDIR)/sbin/sched
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 sched $@
	@rm ./sched

vfs.o: $(DESTDIR)/sbin/vfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 vfs $@
	@rm ./vfs

systask.o: $(DESTDIR)/sbin/systask
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 systask $@
	@rm ./systask

tty.o: $(DESTDIR)/sbin/tty
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 tty $@
	@rm ./tty

ramdisk.o: $(DESTDIR)/sbin/ramdisk
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 ramdisk $@
	@rm ./ramdisk

initfs.o: $(DESTDIR)/sbin/initfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 initfs $@
	@rm ./initfs

sysfs.o: $(DESTDIR)/sbin/sysfs
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 sysfs $@
	@rm ./sysfs

ipc.o: $(DESTDIR)/sbin/ipc
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 ipc $@
	@rm ./ipc

pci.o: $(DESTDIR)/sbin/pci
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 pci $@
	@rm ./pci

init.o: $(DESTDIR)/sbin/init
	@echo -e '\tOBJCOPY\tkernel/bootmods/$@'
	@cp $< ./
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 init $@
	@rm ./init
