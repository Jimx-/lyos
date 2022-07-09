#
# Makefile for the Lyos framebuffer DRM driver.

SRCS	+= main.c
LIBS	= drmdriver chardriver devman pciutil sysfs lyos

PROG	= fbdrm

CFLAGS		+= -I$(CURDIR)
SERVERCFLAGS += -I$(CURDIR)

include lyos.service.mk
