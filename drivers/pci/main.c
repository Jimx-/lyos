/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/pci.h"

#define DEBUG

#ifdef DEBUG
#define PRINTL(x) printl(x)
#else
#define PRINTL(x)
#endif

extern struct pci_device pci_device_table[];

PRIVATE int pci_bus_probe();
PRIVATE void pci_scan_devices(int bus_nr);

PUBLIC void task_pci()
{
	pci_init();
	while(1){
	}
}

PUBLIC void pci_init()
{
	pci_scan_devices(0);
}

PRIVATE int pci_bus_probe() 
{
	unsigned long init = 0x80000000;
	out_long(PCI_CTRL, init);
	init = in_long(PCI_DATA);
	if(init == 0xFFFFFFFF)   
		return 0;
	return 1;
}

PRIVATE u16 pci_read_word(u32 bus, u32 slot, u32 func, u16 offset) 
{
	u32 address = (u32)((bus << 16) | (slot << 11) |
			(func << 8) | (offset & 0xFC) | ((u32)0x80000000));
	out_long(PCI_CTRL, address);
	return (u16)((in_long(PCI_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

PRIVATE void pci_scan_devices(int bus_nr)
{
	printl("PCI: Scanning PCI devices...\n");

	int i = 0, func = 0;
	for(i = 0; i < 32; i++) 
	{
		for (func = 0; func < 8; func++) {
			u16 vendor = pci_read_word(bus_nr, i, func, 0);
			u16 device = pci_read_word(bus_nr, i, func, 2);
		                     
			if(vendor == 0xFFFF)        
				continue;

			char * name = pci_dev_name(vendor, device);
			if (name) {
				printl("PCI: %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus_nr, i, func, vendor, device, name);
			} else {
				printl("PCI: %d.%02x.%x: Unknown device (0x%04x:0x%04x)\n", bus_nr, i, func, vendor, device);
			}
			
		}
	}
	return;
}
