#
# Makefile for device manager.

SRCS 	= main.c ddmap.c global.c bus.c device.c
LIBS 	= lyos sysfs

PROG 	= devman
EXTRACFLAGS = -Xlinker -Ttext -Xlinker 0x80000000

include lyos.service.mk
