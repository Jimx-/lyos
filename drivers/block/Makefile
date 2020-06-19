#
# Makefile for the Lyos block device drivers
#

SUBDIRS = ramdisk ata

ifeq ($(CONFIG_VIRTIO_BLK),y)
	SUBDIRS += virtio-blk
endif

include lyos.subdirs.mk
