#
# Makefile for the Lyos framebuffer driver.

include arch/$(ARCH)/Makefile

SRCS	+= fb.c
LIBS	= chardriver devman pciutil sysfs lyos

PROG	= fb

CFLAGS		+= -I$(CURDIR)
SERVERCFLAGS += -I$(CURDIR)

include lyos.service.mk
