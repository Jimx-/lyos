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
    
#include "type.h"
#include "stdio.h"
#include "unistd.h"
#include "config.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "pci.h"

#define DEBUG

#ifdef DEBUG
#define PRINTL(x) printl(x)
#else
#define PRINTL(x)
#endif

/*
PRIVATE void pci_read_config_byte(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned char *val);
PRIVATE void pci_read_config_word(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short *val);
PRIVATE void pci_read_config_dword(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned int *val);
PRIVATE void pci_write_config_dword(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned int val);
PRIVATE void pci_write_config_word(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short val);
PRIVATE void pci_write_config_byte(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short val);

PRIVATE void scan_pci(int bus);
*/

PUBLIC void task_pci()
{
	pci_init();
	while(1){
	}
}

/*
PRIVATE void pci_read_config_byte(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned char *val)
{
	unsigned long lbus = (unsigned long)bus;
     	unsigned long ldev = (unsigned long)dev;
	unsigned long lfn = (unsigned long)func;

     	out_long(PCI_CTRL, (0x80000000 | ((lbus)<<16) |((ldev)<<11) | ((lfn)<<8) | (offset & ~0x3)));
     	*val = in_long(PCI_DATA) >> ((offset & 3) * 8);
}


PRIVATE void pci_read_config_word(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short *val)
{
	unsigned long lbus = (unsigned long)bus;
     	unsigned long ldev = (unsigned long)dev;
	unsigned long lfn = (unsigned long)func;

     	out_long(PCI_CTRL, (0x80000000 | ((lbus)<<16) |((ldev)<<11) | ((lfn)<<8) | (offset & 0xfc)));
     	*val = in_long(PCI_DATA) >> ((offset & 3) * 8);
}


PRIVATE void pci_read_config_dword(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned int *val)
{
     	out_long(PCI_CTRL, (0x80000000 | ((bus)<<16) |((dev)<<11) | ((func)<<8) | (offset)));
     	*val = in_long(PCI_DATA);
}

PRIVATE void pci_write_config_dword(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned int val)
{
       out_long(PCI_CTRL, (0x80000000 | ((bus)<<16) |((dev)<<11) | ((func)<<8) | (offset)));
       out_long(PCI_DATA, val);
}


PRIVATE void pci_write_config_word(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short val)
{
       unsigned long tmp;

       out_long(PCI_CTRL, (0x80000000 | ((bus)<<16) |((dev)<<11) | ((func)<<8) | (offset & ~0x3)));
       tmp = in_long(PCI_DATA);
       tmp &= ~(0xffff << ((offset & 0x3) * 8));
       tmp |= (val << ((offset & 0x3) * 8));
       out_long(PCI_DATA, tmp);
}


PRIVATE void pci_write_config_byte(unsigned char bus, unsigned char dev, unsigned char func, unsigned char offset, unsigned short val)
{
       unsigned long tmp;

       out_long(PCI_CTRL, (0x80000000 | ((bus)<<16) |((dev)<<11) |((func)<<8) | (offset & ~0x3)));      
       tmp = in_long(PCI_DATA);
       tmp &= ~(0xff << ((offset & 0x3) * 8));
       tmp |= (val << ((offset & 0x3) * 8));
       out_long(PCI_DATA, tmp);
} 

PRIVATE void scan_pci(int bus)
{
	unsigned int dev, fn;
	//unsigned char type;
	unsigned short vendor;

	for (dev = 0; dev <= 0xf; ++dev)
		for (fn = 0; fn <=0xf; ++fn){
			pci_read_config_word(0,dev,fn,0,&vendor);
	}	
}*/

PUBLIC void pci_init()
{
	/*printl("Probing PCI hardware...\n");
	scan_pci(0);*/
}
     
