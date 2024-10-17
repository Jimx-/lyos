PROG	= usbd

SRCS-$(CONFIG_USB_PCI) += hcd-pci.c
SRCS-$(CONFIG_USB_OHCI_HCD) += ohci-hcd.c
SRCS-$(CONFIG_USB_OHCI_HCD_PCI) += ohci-pci.c
SRCS-$(CONFIG_USB_DWC2) += dwc2.c

SRCS	= main.c worker.c usb.c message.c hcd.c hub.c config.c $(SRCS-y)
LIBS	= blockdriver coro devman sysfs lyos

ifeq ($(CONFIG_COMMON_CLK),y)
	LIBS := clk $(LIBS)
endif

ifeq ($(CONFIG_USB_PCI),y)
	LIBS := pciutil $(LIBS)
endif

ifeq ($(CONFIG_OF),y)
	LIBS := of fdt $(LIBS)
endif

include lyos.service.mk
