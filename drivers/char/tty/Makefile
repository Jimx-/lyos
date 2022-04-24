#
# Makefile for the Lyos tty driver.

include arch/$(ARCH)/Makefile

SRCS	+= tty.c global.c keyboard.c serial.c
LIBS	= chardriver devman sysfs lyos

ifeq ($(CONFIG_OF),y)
	LIBS += of fdt
endif

PROG	= tty

CFLAGS	+= -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)
SERVERCFLAGS += -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)

include lyos.service.mk
