#
# Makefile for Lyos libraries.

SUBDIRS		= libbdev libexec liblyos libsysfs libfsdriver libinputdriver \
			  libblockdriver libchardriver libmemfs libdevman libpthread \
			  libcoro libsockdriver

ifeq ($(CONFIG_OF),y)
	SUBDIRS += libof libfdt
endif

ifeq ($(CONFIG_VIRTIO),y)
	SUBDIRS += libvirtio
endif

ifeq ($(CONFIG_DRM),y)
	SUBDIRS += libdrmdriver
endif

include lyos.subdirs.mk
