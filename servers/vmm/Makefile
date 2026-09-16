#
# Makefile for the Lyos virtual machine monitor.
#

SRCS	= main.c vm.c run.c
ifneq ($(filter y,$(CONFIG_INTEL_VMX) $(CONFIG_AMD_SVM)),)
SRCS += arch/x86/vmx.c
endif
SERVERCFLAGS += -I$(CURDIR)
LIBS	= lyos

PROG	= vmm

include lyos.service.mk
