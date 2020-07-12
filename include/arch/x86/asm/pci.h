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

#ifndef _ARCH_PCI_H_
#define _ARCH_PCI_H_

#define PCI_CTRL 0xCF8
#define PCI_DATA 0xCFC

#define PCI_VID          0x00   /* Vendor ID, 16-bit */
#define PCI_DID          0x02   /* Device ID, 16-bit */
#define PCI_CR           0x04   /* Command Register, 16-bit */
#define PCI_CR_MAST_EN   0x0004 /* Enable Busmaster Access */
#define PCI_CR_MEM_EN    0x0002 /* Enable Mem Cycles */
#define PCI_CR_IO_EN     0x0001 /* Enable I/O Cycles */
#define PCI_SR           0x06   /* PCI status, 16-bit */
#define PSR_SSE          0x4000 /* Signaled System Error */
#define PSR_RMAS         0x2000 /* Received Master Abort Status */
#define PSR_RTAS         0x1000 /* Received Target Abort Status */
#define PSR_CAPPTR       0x0010 /* Capabilities list */
#define PCI_REV          0x08   /* Revision ID */
#define PCI_PIFR         0x09   /* Prog. Interface Register */
#define PCI_SCR          0x0A   /* Sub-Class Register */
#define PCI_BCR          0x0B   /* Base-Class Register */
#define PCI_CLS          0x0C   /* Cache Line Size */
#define PCI_LT           0x0D   /* Latency Timer */
#define PCI_HEADT        0x0E   /* Header type, 8-bit */
#define PHT_MASK         0x7F   /* Header type mask */
#define PHT_NORMAL       0x00
#define PHT_BRIDGE       0x01
#define PHT_CARDBUS      0x02
#define PHT_MULTIFUNC    0x80       /* Multiple functions */
#define PCI_BIST         0x0F       /* Built-in Self Test */
#define PCI_BAR          0x10       /* Base Address Register */
#define PCI_BAR_IO       0x00000001 /* Reg. refers to I/O space */
#define PCI_BAR_TYPE     0x00000006 /* Memory BAR type */
#define PCI_TYPE_32      0x00000000 /* 32-bit BAR */
#define PCI_TYPE_32_1M   0x00000002 /* 32-bit below 1MB (legacy) */
#define PCI_TYPE_64      0x00000004 /* 64-bit BAR */
#define PCI_BAR_PREFETCH 0x00000008 /* Memory is prefetchable */
#define PCI_BAR_IO_MASK  0xFFFFFFFC /* I/O address mask */
#define PCI_BAR_MEM_MASK 0xFFFFFFF0 /* Memory address mask */
#define PCI_BAR_2        0x14       /* Base Address Register */
#define PCI_BAR_3        0x18       /* Base Address Register */
#define PCI_BAR_4        0x1C       /* Base Address Register */
#define PCI_BAR_5        0x20       /* Base Address Register */
#define PCI_BAR_6        0x24       /* Base Address Register */
#define PCI_CBCISPTR     0x28       /* Cardbus CIS Pointer */
#define PCI_SUBVID       0x2C       /* Subsystem Vendor ID */
#define PCI_SUBDID       0x2E       /* Subsystem Device ID */
#define PCI_EXPROM       0x30       /* Expansion ROM Base Address */
#define PCI_CAPPTR       0x34       /* Capabilities Pointer */
#define PCI_CP_MASK      0xfc       /* Lower 2 bits should be ignored */
#define PCI_ILR          0x3C       /* Interrupt Line Register */
#define PCI_ILR_UNKNOWN  0xFF       /* IRQ is unassigned or unknown */
#define PCI_IPR          0x3D       /* Interrupt Pin Register */
#define PCI_MINGNT       0x3E       /* Min Grant */
#define PCI_MAXLAT       0x3F       /* Max Latency */

#define PCI_BCR_MASS_STORAGE 0x01 /* Mass Storage class */
#define PCI_MS_IDE           0x01 /* IDE storage class */
#define PCI_IDE_PRI_NATIVE     \
    0x01 /* Primary channel is \
          * in native mode.    \
          */
#define PCI_IDE_SEC_NATIVE       \
    0x04 /* Secondary channel is \
          * in native mode.      \
          */

/* Capability lists */

#define PCI_CAP_LIST_ID   0    /* Capability ID */
#define PCI_CAP_ID_PM     0x01 /* Power Management */
#define PCI_CAP_ID_AGP    0x02 /* Accelerated Graphics Port */
#define PCI_CAP_ID_VPD    0x03 /* Vital Product Data */
#define PCI_CAP_ID_SLOTID 0x04 /* Slot Identification */
#define PCI_CAP_ID_MSI    0x05 /* Message Signalled Interrupts */
#define PCI_CAP_ID_CHSWP  0x06 /* CompactPCI HotSwap */
#define PCI_CAP_ID_PCIX   0x07 /* PCI-X */
#define PCI_CAP_ID_HT     0x08 /* HyperTransport */
#define PCI_CAP_ID_VNDR   0x09 /* Vendor-Specific */
#define PCI_CAP_ID_DBG    0x0A /* Debug port */
#define PCI_CAP_ID_CCRC   0x0B /* CompactPCI Central Resource Control */
#define PCI_CAP_ID_SHPC   0x0C /* PCI Standard Hot-Plug Controller */
#define PCI_CAP_ID_SSVID  0x0D /* Bridge subsystem vendor/device ID */
#define PCI_CAP_ID_AGP3   0x0E /* AGP Target PCI-PCI bridge */
#define PCI_CAP_ID_SECDEV 0x0F /* Secure Device */
#define PCI_CAP_ID_EXP    0x10 /* PCI Express */
#define PCI_CAP_ID_MSIX   0x11 /* MSI-X */
#define PCI_CAP_ID_SATA   0x12 /* SATA Data/Index Conf. */
#define PCI_CAP_ID_AF     0x13 /* PCI Advanced Features */
#define PCI_CAP_ID_EA     0x14 /* PCI Enhanced Allocation */
#define PCI_CAP_ID_MAX    PCI_CAP_ID_EA
#define PCI_CAP_LIST_NEXT 1 /* Next capability in the list */
#define PCI_CAP_FLAGS     2 /* Capability defined flags (16 bits) */
#define PCI_CAP_SIZEOF    4

#endif
