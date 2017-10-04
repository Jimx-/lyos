#
# Makefile for the Lyos framebuffer driver.

include arch/$(ARCH)/Makefile

SRCS 	+= fb.c
LIBS	= lyos chardriver devman sysfs

PROG	= fb

CFLAGS 	+= -I$(CURDIR)
SERVERCFLAGS += -I$(CURDIR)

include lyos.service.mk
