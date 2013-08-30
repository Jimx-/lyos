#
# Makefile for the Lyos firmware drivers
#

ifeq ($(wildcard $(SRCDIR)/.config),) 
else
	include $(SRCDIR)/.config
endif

ASMKFLAGS	= -I $(INCDIR)/ -I $(ARCHINCDIR)/ -f elf

obj-y = _firmware.o
obj-$(CONFIG_FIRMWARE_DMI) += dmi.o

FIRMWARE = firmware.o

everything : realclean $(FIRMWARE)

realclean :
	@rm -f $(FIRMWARE) $(obj-y)

$(FIRMWARE): $(obj-y)
	@echo -e '\tLD\tdrivers/firmware/$@'
	@$(LD) -r -o $@ $^

_firmware.o: firmware.c
	@echo -e '\tCC\tdrivers/firmware/$@'
	@$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	@echo -e '\tCC\tdrivers/firmware/$@'
	@$(CC) $(CFLAGS) -o $@ $<
