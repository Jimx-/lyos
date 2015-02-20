#
# Makefile for Lyos ramdisk
#

INITRD = $(ARCHDIR)/initrd.tar

.PHONY: everything realclean
	
everything : realclean $(INITRD)

realclean :
	@rm -f $(INITRD)

$(INITRD):
	@(cd ../userspace; make)
	@cp $(DESTDIR)/sbin/init sbin/
	@cp $(DESTDIR)/sbin/ext2fs sbin/
	@cp $(DESTDIR)/sbin/ata sbin/
	@cp $(DESTDIR)/usr/bin/getty usr/bin/getty
	@cp $(DESTDIR)/usr/bin/login usr/bin/login
	@cp $(DESTDIR)/bin/sh bin/
	@cp ../sysroot/etc/* etc/
	@touch .root
	@(cd ../scripts ; bash create-ramdisk-dev.sh)
	@tar -cvf $(INITRD) .root bin/* sbin/* dev/* etc/* usr/bin/* > /dev/null
	@rm .root
	@cp -f $(INITRD) $(DESTDIR)/boot/