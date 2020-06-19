#
# Makefile for Lyos libraries.

SUBDIRS		= libbdev libexec liblyos libsysfs libfsdriver libinputdriver \
			  libblockdriver libchardriver libmemfs libdevman libpthread \
			  libcoro

ifeq ($(CONFIG_OF),y)
	SUBDIRS += libof libfdt
endif

ifeq ($(CONFIG_VIRTIO),y)
	SUBDIRS += libvirtio
endif

include lyos.subdirs.mk
