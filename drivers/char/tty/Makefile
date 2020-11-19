#
# Makefile for the Lyos tty driver.

include arch/$(ARCH)/Makefile

SRCS	+= tty.c global.c keyboard.c
LIBS	= lyos chardriver devman sysfs

PROG	= tty

CFLAGS	+= -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)
SERVERCFLAGS += -I$(CURDIR) -I$(CURDIR)/arch/$(ARCH)

include lyos.service.mk
