#
# Makefile for the Lyos virtual machine monitor.
#

SRCS	= main.c vm.c run.c
ifeq ($(CONFIG_INTEL_VMX),y)
SRCS += arch/x86/vmx.c
endif
SERVERCFLAGS += -I$(CURDIR)
LIBS	= lyos

PROG	= vmm

include lyos.service.mk
