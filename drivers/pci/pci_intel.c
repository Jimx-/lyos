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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/portio.h>
#include <lyos/service.h>

#include <asm/pci.h>
#include "pci.h"
#include <asm/pci_intel.h>

static u8 pcii_read_u8(u32 bus, u32 devfn, u16 offset)
{
    u8 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_inb(PCI_DATA + (offset & 3), &v);

    return v;
}

u16 pcii_read_u16(u32 bus, u32 devfn, u16 offset)
{
    u32 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_inl(PCI_DATA, &v);

    return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

static u32 pcii_read_u32(u32 bus, u32 devfn, u16 offset)
{
    u32 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_inl(PCI_DATA, &v);

    return v;
}

static void pcii_write_u8(u32 bus, u32 devfn, u16 offset, u8 value)
{
    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_outb(PCI_DATA, value);
}

static void pcii_write_u16(u32 bus, u32 devfn, u16 offset, u16 value)
{
    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_outw(PCI_DATA, value);
}

static void pcii_write_u32(u32 bus, u32 devfn, u16 offset, u32 value)
{
    portio_outl(PCII_CTRL, PCII_SELREG(bus, devfn, offset));
    portio_outl(PCI_DATA, value);
}

static u8 pcii_rreg_u8(struct pcibus* bus, unsigned int devfn, int port)
{
    return pcii_read_u8(bus->busnr, devfn, port);
}

static u16 pcii_rreg_u16(struct pcibus* bus, unsigned int devfn, int port)
{
    return pcii_read_u16(bus->busnr, devfn, port);
}

static u32 pcii_rreg_u32(struct pcibus* bus, unsigned int devfn, int port)
{
    return pcii_read_u32(bus->busnr, devfn, port);
}

static void pcii_wreg_u8(struct pcibus* bus, unsigned int devfn, int port,
                         u8 value)
{
    return pcii_write_u8(bus->busnr, devfn, port, value);
}

static void pcii_wreg_u16(struct pcibus* bus, unsigned int devfn, int port,
                          u16 value)
{
    return pcii_write_u16(bus->busnr, devfn, port, value);
}

static void pcii_wreg_u32(struct pcibus* bus, unsigned int devfn, int port,
                          u32 value)
{
    return pcii_write_u32(bus->busnr, devfn, port, value);
}

static const struct pci_ops pci_intel_ops = {
    .rreg_u8 = pcii_rreg_u8,
    .rreg_u16 = pcii_rreg_u16,
    .rreg_u32 = pcii_rreg_u32,
    .wreg_u8 = pcii_wreg_u8,
    .wreg_u16 = pcii_wreg_u16,
    .wreg_u32 = pcii_wreg_u32,
};

void pci_intel_init()
{
    u32 bus, devfn;

    bus = 0;
    devfn = 0;

    u16 vendor = pcii_read_u16(bus, devfn, PCI_VID);
    u16 device = pcii_read_u16(bus, devfn, PCI_DID);

    if (vendor == 0xffff && device == 0xffff) return;

    pci_scan_bus(0, &pci_intel_ops, NULL);
}
