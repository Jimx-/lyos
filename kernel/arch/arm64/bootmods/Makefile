####################################
# Makefile for Lyos boot modules.  #
####################################

BOOTMODSBINS	= mm pm servman devman sched vfs systask tty ramdisk \
					initfs sysfs ipc netlink init

BOOTMODS		= bootmods.o

everything : realclean $(BOOTMODS)

realclean :
	@rm -f $(BOOTMODS) $(BOOTMODSBINS)

$(BOOTMODS): bootmods.S $(BOOTMODSBINS)
	@echo -e '\tCC\tkernel/bootmods/$@'
	@$(CC) $(CFLAGS) -c -D__ASSEMBLY__ -o $@ bootmods.S

mm : $(DESTDIR)/sbin/mm
	@cp $< $@

pm : $(DESTDIR)/sbin/pm
	@cp $< $@

servman : $(DESTDIR)/sbin/servman
	@cp $< $@

devman : $(DESTDIR)/sbin/devman
	@cp $< $@

sched : $(DESTDIR)/sbin/sched
	@cp $< $@

vfs : $(DESTDIR)/sbin/vfs
	@cp $< $@

systask : $(DESTDIR)/sbin/systask
	@cp $< $@

tty : $(DESTDIR)/sbin/tty
	@cp $< $@

ramdisk : $(DESTDIR)/sbin/ramdisk
	@cp $< $@

initfs : $(DESTDIR)/sbin/initfs
	@cp $< $@

sysfs : $(DESTDIR)/sbin/sysfs
	@cp $< $@

ipc : $(DESTDIR)/sbin/ipc
	@cp $< $@

netlink : $(DESTDIR)/sbin/netlink
	@cp $< $@

init : $(DESTDIR)/sbin/init
	@cp $< $@
