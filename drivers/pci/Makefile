#
# Makefile for pci driver.

SRCS	= main.c pci.c pci_table.c pci_dev_attr.c
LIBS	= devman lyos

ifeq ($(ARCH),x86)
	SRCS += pci_intel.c
endif

ifeq ($(CONFIG_OF),y)
	SRCS += pci_host_generic.c pci_ecam.c
	LIBS := of fdt $(LIBS)
endif

PROG	= pci

include lyos.service.mk
