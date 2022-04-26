#
# Makefile for the Lyos block device drivers
#

SUBDIRS = ramdisk

ifeq ($(CONFIG_BLK_ATA_GENERIC),y)
	SUBDIRS += ata
endif

ifeq ($(CONFIG_VIRTIO_BLK),y)
	SUBDIRS += virtio-blk
endif

ifeq ($(CONFIG_BLK_DEV_NVME),y)
	SUBDIRS += nvme
endif

ifeq ($(CONFIG_MMC),y)
	SUBDIRS += mmc
endif

include lyos.subdirs.mk
