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
	@cp -f $(DESTDIR)/sbin/init sbin/
	@cp -f $(DESTDIR)/sbin/ext2fs sbin/
	@cp -f $(DESTDIR)/sbin/pci sbin/
	@cp -f $(DESTDIR)/sbin/virtio-blk sbin/
	@cp -f $(DESTDIR)/sbin/virtio-gpu sbin/
	@cp -f $(DESTDIR)/sbin/virtio-net sbin/
	@cp -f $(DESTDIR)/sbin/procfs sbin/
	@cp -f $(DESTDIR)/sbin/input sbin/
ifneq ("$(wildcard $(DESTDIR)/sbin/atkbd)","")
	@cp -f $(DESTDIR)/sbin/atkbd sbin/
endif
ifneq ("$(wildcard $(DESTDIR)/sbin/mmcblk)","")
	@cp -f $(DESTDIR)/sbin/mmcblk sbin/
endif
ifneq ("$(wildcard $(DESTDIR)/sbin/clk)","")
	@cp -f $(DESTDIR)/sbin/clk sbin/
endif
ifneq ("$(wildcard $(DESTDIR)/sbin/usbd)","")
	@cp -f $(DESTDIR)/sbin/usbd sbin/
endif
	@cp -f $(DESTDIR)/sbin/uds sbin/
	@cp -f $(DESTDIR)/sbin/tmpfs sbin/
	@cp -f $(DESTDIR)/sbin/pty sbin/
	@cp -f $(DESTDIR)/sbin/devpts sbin/
	@cp -f $(DESTDIR)/sbin/inet sbin/
	@cp -f $(DESTDIR)/sbin/fbdrm sbin/
	@cp -f $(DESTDIR)/usr/bin/getty usr/bin/
	@cp -f $(DESTDIR)/usr/bin/login usr/bin/
ifneq ("$(wildcard $(DESTDIR)/usr/bin/sleep)","")
	@cp -f $(DESTDIR)/usr/bin/sleep usr/bin/
endif
	@cp -f $(DESTDIR)/bin/sh bin/
	@cp -f $(DESTDIR)/bin/service bin/
	@cp -f $(DESTDIR)/bin/mount bin/
	@cp -f $(DESTDIR)/lib/ld-lyos.so lib/
	@cp -f $(DESTDIR)/usr/lib/libc.so usr/lib
	@cp -f $(DESTDIR)/usr/lib/libm.so usr/lib
	@cp -f $(DESTDIR)/usr/lib/libcjson.so* usr/lib
	@cp -rf ../sysroot/etc/* etc/
	@cp -f ../sysroot/etc/rc.$(SUBARCH) etc/rc
	@touch .root
	@tar -cvf $(INITRD) .root bin lib sbin etc usr > /dev/null
	@rm .root
	@cp -f $(INITRD) $(DESTDIR)/boot/
