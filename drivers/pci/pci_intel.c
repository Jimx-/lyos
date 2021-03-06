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
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/service.h>

#include <asm/pci.h>
#include "pci.h"
#include <asm/pci_intel.h>

u8 pcii_read_u8(u32 bus, u32 slot, u32 func, u16 offset)
{
    u8 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, slot, func, offset));
    portio_inb(PCI_DATA + (offset & 3), &v);

    return v;
}

u16 pcii_read_u16(u32 bus, u32 slot, u32 func, u16 offset)
{
    u32 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, slot, func, offset));
    portio_inl(PCI_DATA, &v);

    return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

u32 pcii_read_u32(u32 bus, u32 slot, u32 func, u16 offset)
{
    u32 v;

    portio_outl(PCII_CTRL, PCII_SELREG(bus, slot, func, offset));
    portio_inl(PCI_DATA, &v);

    return v;
}

void pcii_write_u16(u32 bus, u32 slot, u32 func, u16 offset, u16 value)
{
    portio_outl(PCII_CTRL, PCII_SELREG(bus, slot, func, offset));
    portio_outw(PCI_DATA, value);
}

void pcii_write_u32(u32 bus, u32 slot, u32 func, u16 offset, u32 value)
{
    portio_outl(PCII_CTRL, PCII_SELREG(bus, slot, func, offset));
    portio_outl(PCI_DATA, value);
}

u8 pcii_rreg_u8(u32 busind, u32 devind, u16 port)
{
    return pcii_read_u8(pcibus[busind].busnr, pcidev[devind].dev,
                        pcidev[devind].func, port);
}

u16 pcii_rreg_u16(u32 busind, u32 devind, u16 port)
{
    return pcii_read_u16(pcibus[busind].busnr, pcidev[devind].dev,
                         pcidev[devind].func, port);
}

u32 pcii_rreg_u32(u32 busind, u32 devind, u16 port)
{
    return pcii_read_u32(pcibus[busind].busnr, pcidev[devind].dev,
                         pcidev[devind].func, port);
}

void pcii_wreg_u16(u32 busind, u32 devind, u16 port, u16 value)
{
    return pcii_write_u16(pcibus[busind].busnr, pcidev[devind].dev,
                          pcidev[devind].func, port, value);
}

void pcii_wreg_u32(u32 busind, u32 devind, u16 port, u32 value)
{
    return pcii_write_u32(pcibus[busind].busnr, pcidev[devind].dev,
                          pcidev[devind].func, port, value);
}
