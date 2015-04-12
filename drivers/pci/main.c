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
#include <lyos/portio.h>
#include <lyos/service.h>

#include <pci.h>
#include "pci.h"

#define PCI_DEBUG

#ifdef PCI_DEBUG
#define DBGPRINT(x) printl(x)
#else
#define DBGPRINT(x)
#endif

PUBLIC struct pcibus pcibus[NR_PCIBUS];
PRIVATE int nr_pcibus = 0;

PUBLIC struct pcidev pcidev[NR_PCIDEV];
PRIVATE int nr_pcidev = 0;

PRIVATE int pci_init();
PRIVATE void pci_intel_init();
PRIVATE void pci_probe_bus(int busind);
PRIVATE u16 pci_read_attr_u16(int devind, int port);

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

PRIVATE int get_busind(int busnr)
{
	int i;
	for (i = 0; i < nr_pcibus; i++) {
		if (pcibus[i].busnr == busnr) return i;
	}

	panic("get_busind failed\n");
}

PRIVATE u16 pci_read_attr_u8(int devind, int port)
{
	int busnr = pcidev[devind].busnr;
	int busind = get_busind(busnr);
	return pcibus[busind].rreg_u8(busind, devind, port);
}

PRIVATE u16 pci_read_attr_u16(int devind, int port)
{
	int busnr = pcidev[devind].busnr;
	int busind = get_busind(busnr);
	return pcibus[busind].rreg_u16(busind, devind, port);
}

PRIVATE int pci_init()
{
	pci_intel_init();

	return 0;
}

PRIVATE void pci_intel_init()
{
	u32 bus, dev, func;
	
	bus = 0;
	dev = 0;
	func = 0;

	u16 vendor = pcii_read_u16(bus, dev, func, PCI_VID);
	u16 device = pcii_read_u16(bus, dev, func, PCI_DID);

	if (vendor == 0xffff && device == 0xffff) return;

	if (nr_pcibus >= NR_PCIBUS) return;

	int busind = nr_pcibus++;
	pcibus[busind].busnr = 0;
	pcibus[busind].rreg_u8 = pcii_rreg_u8;
	pcibus[busind].rreg_u16 = pcii_rreg_u16;

	char * name = pci_dev_name(vendor, device);
	if (name) {
		printl("pci %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus, dev, func, vendor, device, name);
	} else {
		printl("pci %d.%02x.%x: Unknown device (0x%04x:0x%04x)\n", bus, dev, func, vendor, device);
	}

	pci_probe_bus(busind);
}

PRIVATE void pci_probe_bus(int busind)
{
	u8 bus_nr = pcibus[busind].busnr;

	int devind = nr_pcidev;

	int i = 0, func = 0;
	for(i = 0; i < 32; i++) 
	{
		for (func = 0; func < 8; func++) {
			pcidev[devind].busnr = bus_nr;
			pcidev[devind].dev = i;
			pcidev[devind].func = func;

			u16 vendor = pci_read_attr_u16(devind, PCI_VID);
			u16 device = pci_read_attr_u16(devind, PCI_DID);
		                     
			if(vendor == 0xffff) {
				if (func == 0) break;

				continue;
			}
			
			u8 baseclass = pci_read_attr_u8(devind, PCI_BCR);
			u8 subclass = pci_read_attr_u8(devind, PCI_SCR);

			devind = nr_pcidev;
			nr_pcidev++;

			pcidev[devind].vid = vendor;
			pcidev[devind].did = device;
			pcidev[devind].baseclass = baseclass;
			pcidev[devind].subclass = subclass;

			char * name = pci_dev_name(vendor, device);
			if (name) {
				printl("pci %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus_nr, i, func, vendor, device, name);
			} else {
				printl("pci %d.%02x.%x: Unknown device (0x%04x:0x%04x)\n", bus_nr, i, func, vendor, device);
			}
		}
	}
}
