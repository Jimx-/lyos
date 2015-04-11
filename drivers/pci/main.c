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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/pci.h"
#include <lyos/portio.h>
#include <lyos/service.h>

#define DEBUG

#ifdef DEBUG
#define PRINTL(x) printl(x)
#else
#define PRINTL(x)
#endif

extern struct pci_device pci_device_table[];

PRIVATE int pci_init();
PRIVATE int pci_bus_probe();
PRIVATE void pci_scan_devices(int bus_nr);

PUBLIC int main()
{
	serv_register_init_fresh_callback(pci_init);
	serv_init();

	MESSAGE msg;
	
	while(TRUE) {
        send_recv(RECEIVE, ANY, &msg);
	}

	return 0;
}

PRIVATE int pci_init()
{
	pci_scan_devices(0);

	return 0;
}

PRIVATE int pci_bus_probe() 
{
	unsigned long init = 0x80000000;

	portio_outl(PCI_CTRL, init);
	portio_inl(PCI_DATA, &init);
	
	if(init == 0xFFFFFFFF)
		return 0;
	return 1;
}

PRIVATE u16 pci_read_word(u32 bus, u32 slot, u32 func, u16 offset) 
{
	u32 address = (u32)((bus << 16) | (slot << 11) |
			(func << 8) | (offset & 0xFC) | ((u32)0x80000000));

	portio_outl(PCI_CTRL, address);
	
	u32 v;
	portio_inl(PCI_DATA, &v);

	return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
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
				printl("pci %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus_nr, i, func, vendor, device, name);
			} else {
				printl("pci %d.%02x.%x: Unknown device (0x%04x:0x%04x)\n", bus_nr, i, func, vendor, device);
			}
			
		}
	}

	return;
}
