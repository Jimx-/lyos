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

PRIVATE int pci_bus_probe();
PRIVATE void pci_scan_devices(int bus_nr);

PUBLIC void task_pci()
{
	pci_init();
	while(1){
	}
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

PRIVATE void pci_scan_devices(int bus_nr)
{
	unsigned long conf_reg = 0x80000000;
	unsigned long tmp = 0L;

	printl("Scaning PCI device.\n");
	tmp = bus_nr;
	tmp &= 0x000000FF; 
	conf_reg += (tmp << 16);

	int i = 0;
	for(i = 0; i < 0x100; i++) 
	{
		conf_reg &= 0xFFFF0000;
		conf_reg += (i << 8);  
		                             
		out_long(PCI_CTRL, conf_reg);
		tmp = in_long(PCI_DATA);
		if(0xFFFFFFFF == tmp)        
			continue;
		printl("A PCI device found.\n");
	}
	return;
}

PUBLIC void pci_init()
{
	pci_scan_devices(0);
}
     
