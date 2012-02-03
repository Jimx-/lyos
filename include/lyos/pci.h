/*  (c)Copyright 2011 Jimx

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

#ifndef _PCI_H
#define _PCI_H

#define PCI_CTRL     0xCF8
#define PCI_DATA     0xCFC

struct pci_bus{
	u8 number;

	char name[48];
	unsigned short vendor;
};

struct pci_dev{
	struct pci_dev * next;

	unsigned char dev;
	unsigned char func;
	unsigned short vendor;

	unsigned char irq;
};

struct pci_driver{
	char * name;
};

#endif
