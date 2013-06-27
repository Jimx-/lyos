LYOSBOOTDIR	= $(ARCHDIR)/boot
LYOSBOOT	= $(LYOSBOOTDIR)/boot.bin $(LYOSBOOTDIR)/hdboot.bin $(LYOSBOOTDIR)/loader.bin \
			$(LYOSBOOTDIR)/hdloader.bin

$(LYOSBOOT):
	@(cd $(LYOSBOOTDIR); make)

image : kernel buildimg

buildimg :
	@echo -e '$(COLORBLUE)Generating the kernel image...$(COLORDEFAULT)'
	dd if=$(LYOSBOOTDIR)/boot.bin of=$(FD) bs=512 count=1 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200" | bc` bs=1 count=446 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200+1FE" | bc` skip=510 bs=1 count=2 conv=notrunc
	sudo mount -o loop $(FD) /mnt/floppy/
	sudo cp -fv $(LYOSBOOTDIR)/loader.bin /mnt/floppy/
	sudo cp -fv $(LYOSKERNEL) /mnt/floppy
	sync
	sudo umount /mnt/floppy

TOOLSDIR = arch/x86/tools

BUILD = $(TOOLSDIR)/build
build:
	@rm -f $(BUILD)
	@make -C $(TOOLSDIR) build

newimage: kernel build
	@rm -f arch/x86/lyos
	@echo -e '\tBUILD\tarch/x86/lyos'
	@$(BUILD) $(LYOSBOOTDIR)/newboot.bin $(LYOSBOOTDIR)/newloader.bin kernel.bin > arch/x86/lyos

newimg : newimage
	dd if=$(ARCHDIR)/lyos of=$(FD) bs=120000 count=1 conv=notrunc

# WARNING: do not use this
floppy:
	@echo -e '$(COLORBLUE)Writing the kernel image to floppy disk...$(COLORDEFAULT)'
	@dd if=a.img of=/dev/sdb bs=1474560
	
