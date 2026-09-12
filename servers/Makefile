#
# Makefile for Lyos servers
#

SUBDIRS = devman init input ipc mm pm sched servman systask vfs

ifeq ($(CONFIG_VMM),y)
SUBDIRS += vmm
endif

include lyos.subdirs.mk

ifneq ($(CONFIG_VMM),y)
install:
	$(Q)rm -f $(DESTDIR)/sbin/vmm
endif
