#
# Makefile for Lyos libraries.

SUBDIRS		= libbdev libexec liblyos libsysfs libfsdriver libinputdriver \
				libblockdriver libchardriver libmemfs libdevman libpthread

ifeq ($(CONFIG_OF),y)
	SUBDIRS += libof libfdt
endif

include lyos.subdirs.mk
