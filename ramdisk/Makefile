#
# Makefile for Lyos ramdisk
#

INITRD = $(ARCHDIR)/initrd.tar

.PHONY: everything realclean
	
everything : realclean $(INITRD)

realclean :
	@rm -f $(INITRD)

$(INITRD):
	$(Q)$(MAKE) -C ../utils $(MAKEFLAGS)
	$(Q)$(MAKE) -C ../utils $(MAKEFLAGS) install
	@cp $(DESTDIR)/sbin/init sbin/
	@cp $(DESTDIR)/sbin/ext2fs sbin/
	@cp $(DESTDIR)/sbin/ata sbin/
	@cp $(DESTDIR)/sbin/procfs sbin/
	@cp $(DESTDIR)/sbin/input sbin/
	@cp $(DESTDIR)/sbin/keyboard sbin/
	@cp $(DESTDIR)/usr/bin/getty usr/bin/
	@cp $(DESTDIR)/usr/bin/login usr/bin/
	@cp $(DESTDIR)/bin/sh bin/
	@cp $(DESTDIR)/bin/service bin/
	@cp $(DESTDIR)/bin/mount bin/
	@cp -rf ../sysroot/etc/* etc/
	@touch .root
	@(cd ../scripts ; bash create-ramdisk-dev.sh)
	@tar -cvf $(INITRD) .root bin/* sbin/* dev/* etc/* usr/bin/* > /dev/null
	@rm .root
	@cp -f $(INITRD) $(DESTDIR)/boot/
