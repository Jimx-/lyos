image : kernel buildimg

buildimg :
	dd if=$(LYOSBOOTDIR)/boot.bin of=$(FD) bs=512 count=1 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200" | bc` bs=1 count=446 conv=notrunc
	dd if=$(LYOSBOOTDIR)/hdboot.bin of=$(HD) seek=`echo "obase=10;ibase=16;\`egrep -e '^ROOT_BASE' $(LYOSBOOTDIR)/include/load.inc | sed -e 's/.*0x//g'\`*200+1FE" | bc` skip=510 bs=1 count=2 conv=notrunc
	sudo mount -o loop $(FD) /mnt/floppy/
	sudo cp -fv $(LYOSBOOTDIR)/loader.bin /mnt/floppy/
	sudo cp -fv kernel.bin /mnt/floppy
	sudo umount /mnt/floppy

