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
#include <errno.h>
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/service.h>
#include <lyos/ipc.h>

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

PRIVATE bus_type_id_t pci_bus_id;

PRIVATE void pci_intel_init();
PRIVATE void pci_probe_bus(int busind);

PRIVATE int get_busind(int busnr)
{
	int i;
	for (i = 0; i < nr_pcibus; i++) {
		if (pcibus[i].busnr == busnr) return i;
	}

	panic("get_busind failed\n");

	return 1;
}

PUBLIC u8 pci_read_attr_u8(int devind, int port)
{
	int busnr = pcidev[devind].busnr;
	int busind = get_busind(busnr);
	return pcibus[busind].rreg_u8(busind, devind, port);
}

PUBLIC u16 pci_read_attr_u16(int devind, int port)
{
	int busnr = pcidev[devind].busnr;
	int busind = get_busind(busnr);
	return pcibus[busind].rreg_u16(busind, devind, port);
}

PUBLIC u32 pci_read_attr_u32(int devind, int port)
{
	int busnr = pcidev[devind].busnr;
	int busind = get_busind(busnr);
	return pcibus[busind].rreg_u32(busind, devind, port);
}

PUBLIC int pci_init()
{
	pci_bus_id = bus_register("pci");
	if (pci_bus_id == BUS_TYPE_ERROR) return 1;

	pci_intel_init();

	int i;
	for (i = 0; i < NR_PRIV_PROCS; i++) {
		pci_acl[i].inuse = 0;
	}

	return 0;
}

PRIVATE device_id_t pci_register_bus(int busind)
{
	struct device_info devinf;
	snprintf(devinf.name, sizeof(devinf.name), "pci%02x", pcibus[busind].busnr);
	devinf.bus = BUS_TYPE_ERROR;
	devinf.parent = NO_DEVICE_ID;

	return device_register(&devinf);
}

PRIVATE device_id_t pci_register_device(int devind)
{
	int busind = get_busind(pcidev[devind].busnr);

	struct device_info devinf;
	snprintf(devinf.name, sizeof(devinf.name), "pci%02x:%02x:%x", pcidev[devind].busnr, pcidev[devind].dev, pcidev[devind].func);
	devinf.bus = pci_bus_id;
	devinf.parent = pcibus[busind].dev_id;

	return device_register(&devinf);
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
	pcibus[busind].dev_id = pci_register_bus(busind);
	if (pcibus[busind].dev_id == NO_DEVICE_ID) {
		nr_pcibus--;
		return;
	}

	pcibus[busind].rreg_u8 = pcii_rreg_u8;
	pcibus[busind].rreg_u16 = pcii_rreg_u16;
	pcibus[busind].rreg_u32 = pcii_rreg_u32;

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
			u8 infclass = pci_read_attr_u8(devind, PCI_PIFR);

			devind = nr_pcidev;
			
			if ((pcidev[devind].dev_id = pci_register_device(devind)) == NO_DEVICE_ID) continue;

			nr_pcidev++;

			pcidev[devind].vid = vendor;
			pcidev[devind].did = device;
			pcidev[devind].baseclass = baseclass;
			pcidev[devind].subclass = subclass;
			pcidev[devind].infclass = infclass;

			char * name = pci_dev_name(vendor, device);
			if (name) {
				printl("pci %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus_nr, i, func, vendor, device, name);
			} else {
				printl("pci %d.%02x.%x: Unknown device (0x%04x:0x%04x)\n", bus_nr, i, func, vendor, device);
			}

			devind = nr_pcidev;
		}
	}
}

PRIVATE int visible(struct pci_acl * acl, int devind)
{
	if (!acl) return TRUE;

	if (!acl->nr_pci_class) return FALSE;

	int i;
	u32 classid = (pcidev[devind].baseclass << 16) | (pcidev[devind].subclass << 8) |
						pcidev[devind].infclass;
	for (i = 0; i < acl->nr_pci_class; i++) {
		if (acl->pci_class[i].classid == (classid & acl->pci_class[i].mask)) return TRUE;
	}

	return FALSE;
}

PUBLIC int _pci_first_dev(struct pci_acl * acl, int * devind, u16 * vid, u16 * did)
{
	int i;

	for (i = 0; i < nr_pcidev; i++) {
		if (!visible(acl, i)) continue;
		break;
	}

	if (i >= nr_pcidev) return ESRCH;

	*devind = i;
	*vid = pcidev[i].vid;
	*did = pcidev[i].did;

	return 0;
}

PUBLIC int _pci_next_dev(struct pci_acl * acl, int * devind, u16 * vid, u16 * did)
{
	int i;

	for (i = *devind + 1; i < nr_pcidev; i++) {
		if (!visible(acl, i)) continue;
		break;
	}

	if (i >= nr_pcidev) return ESRCH;

	*devind = i;
	*vid = pcidev[i].vid;
	*did = pcidev[i].did;

	return 0;
}
