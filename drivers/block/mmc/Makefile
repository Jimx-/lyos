PROG	= mmcblk

SRCS-$(CONFIG_MMC_SDHCI) += sdhci.c

SRCS	= main.c $(SRCS-y)
LIBS	= blockdriver devman sysfs lyos

include lyos.service.mk
