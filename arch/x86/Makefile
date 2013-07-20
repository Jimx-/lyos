LYOSBOOTDIR	= $(ARCHDIR)/boot
LYOSBOOT	= $(LYOSBOOTDIR)/boot.bin $(LYOSBOOTDIR)/hdboot.bin $(LYOSBOOTDIR)/loader.bin \
			$(LYOSBOOTDIR)/hdloader.bin

$(LYOSBOOT):
	@(cd $(LYOSBOOTDIR); make)

image : kernel buildimg

buildimg :
	@echo -e '$(COLORBLUE)Writing the kernel image...$(COLORDEFAULT)'
	sudo mount -t ext2 $(SRCDIR)/lyos-disk.img /mnt/lyos-root
	sudo cp -fv $(LYOSKERNEL) /mnt/lyos-root/boot/
	sync
	sudo umount /mnt/lyos-root

TOOLSDIR = arch/x86/tools

BUILD = $(TOOLSDIR)/build
build:
	@rm -f $(BUILD)
	@make -C $(TOOLSDIR) build

newimage: kernel build
	@rm -f arch/x86/lyos
	@echo -e '\tBUILD\tarch/x86/lyos'
	@$(BUILD) $(LYOSBOOTDIR)/newboot.bin $(LYOSBOOTDIR)/newloader.bin /kernel.bin > arch/x86/lyos

newimg : newimage
	dd if=$(ARCHDIR)/lyos of=$(FD) bs=120000 count=1 conv=notrunc

# WARNING: do not use this
floppy:
	@echo -e '$(COLORBLUE)Writing the kernel image to floppy disk...$(COLORDEFAULT)'
	@dd if=a.img of=/dev/sdb bs=1474560
	
