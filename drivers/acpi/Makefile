#
# Makefile for ACPI service.

SRCS	= main.c osl.c
LIBS	= sysfs lyos
EXTRALIBS = $(DESTDIR)/usr/lib/libacpica.a
SERVERCFLAGS += -I$(DESTDIR)/usr/include -D__linux__

PROG	= acpi

include lyos.service.mk
