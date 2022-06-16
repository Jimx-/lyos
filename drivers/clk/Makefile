#
# Makefile for the common clock framework driver.
#

SRCS-$(CONFIG_CLK_BCM2835) += clk-bcm2835.c

SRCS	= main.c $(SRCS-y)
LIBS	= lyos

ifeq ($(CONFIG_OF),y)
	LIBS := of fdt $(LIBS)
endif

PROG	= clk

include lyos.service.mk
