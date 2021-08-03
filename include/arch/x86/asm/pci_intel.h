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

#ifndef _ARCH_PCI_INTEL_H_
#define _ARCH_PCI_INTEL_H_

#define PCII_CTRL 0xCF8
#define PCII_DATA 0xCFC

#define PCII_SELREG(bus, devfn, offset)                       \
    ((u32)(((bus) << 16) | ((devfn) << 8) | ((offset)&0xFC) | \
           ((u32)0x80000000)))

#endif
