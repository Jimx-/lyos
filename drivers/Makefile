#
# Makefile for Lyos drivers.

SUBDIRS = char hid video

ifeq ($(CONFIG_BLK_DEV),y)
	SUBDIRS += block
endif

ifeq ($(CONFIG_COMMON_CLK),y)
	SUBDIRS += clk
endif

ifeq ($(CONFIG_PCI_DEV),y)
	SUBDIRS += pci
endif

ifeq ($(CONFIG_DRM),y)
	SUBDIRS += gpu/drm
endif

ifeq ($(CONFIG_NETDEVICES),y)
	SUBDIRS += net
endif

ifeq ($(CONFIG_USB_SUPPORT),y)
	SUBDIRS += usb
endif

include lyos.subdirs.mk
