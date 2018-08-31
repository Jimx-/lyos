#
# Makefile for Lyos drivers.

SUBDIRS = char hid video

ifeq ($(CONFIG_BLK_DEV),y)
	SUBDIRS += block
endif 

ifeq ($(CONFIG_PCI_DEV),y)
	SUBDIRS += pci
endif 

include lyos.subdirs.mk
