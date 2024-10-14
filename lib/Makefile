#
# Makefile for Lyos libraries.

SUBDIRS		= libbdev libexec liblyos libsysfs libfsdriver libinputdriver \
					libblockdriver libchardriver libmemfs libdevman libpthread \
					libcoro libsockdriver libdl libnetlink libutil libpciutil \
					libasyncdriver

ifeq ($(CONFIG_OF),y)
	SUBDIRS += libof libfdt
endif

ifeq ($(CONFIG_VIRTIO),y)
	SUBDIRS += libvirtio
endif

ifeq ($(CONFIG_DRM),y)
	SUBDIRS += libdrmdriver
endif

ifeq ($(CONFIG_NET),y)
	SUBDIRS += libnetdriver
endif

ifeq ($(CONFIG_COMMON_CLK),y)
	SUBDIRS += libclk
endif

ifeq ($(CONFIG_USB),y)
	SUBDIRS += libusb
endif

include lyos.subdirs.mk
