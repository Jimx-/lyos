PROG	= mmcblk

SRCS-$(CONFIG_MMC_SDHCI) += sdhci.c
SRCS-$(CONFIG_MMC_SDHCI_OF) += sdhci-of.c
SRCS-$(CONFIG_MMC_SDHCI_IPROC) += sdhci-iproc.c
SRCS-$(CONFIG_MMC_BCM2835) += bcm2835.c

SRCS	= main.c sd.c $(SRCS-y)
LIBS	= blockdriver coro devman sysfs lyos

ifeq ($(CONFIG_OF),y)
	LIBS := of fdt $(LIBS)
endif

include lyos.service.mk
