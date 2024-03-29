/*  This file is part of Lyos.

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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "tty.h"
#include "console.h"
#include <lyos/portio.h>
#include <lyos/sysutils.h>
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/pci.h>
#include "proto.h"
#include "global.h"

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID               0x0
#define VBE_DISPI_INDEX_XRES             0x1
#define VBE_DISPI_INDEX_YRES             0x2
#define VBE_DISPI_INDEX_BPP              0x3
#define VBE_DISPI_INDEX_ENABLE           0x4
#define VBE_DISPI_INDEX_BANK             0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH       0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT      0x7
#define VBE_DISPI_INDEX_X_OFFSET         0x8
#define VBE_DISPI_INDEX_Y_OFFSET         0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa

#define VBE_DISPI_DISABLED    0x00
#define VBE_DISPI_ENABLED     0x01
#define VBE_DISPI_GETCAPS     0x02
#define VBE_DISPI_8BIT_DAC    0x20
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM  0x80

static void bochs_write(u16 reg, u16 val)
{
    portio_outw(VBE_DISPI_IOPORT_INDEX, reg);
    portio_outw(VBE_DISPI_IOPORT_DATA, val);
}

int fbcon_init_bochs(int devind, int x_res, int y_res)
{
    bochs_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bochs_write(VBE_DISPI_INDEX_XRES, x_res);
    bochs_write(VBE_DISPI_INDEX_YRES, y_res);
    bochs_write(VBE_DISPI_INDEX_BPP, 32);
    bochs_write(VBE_DISPI_INDEX_VIRT_HEIGHT, 4096);
    bochs_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED);

    phys_bytes vmem_phys_base = pci_attr_r32(devind, PCI_BAR);
    vmem_phys_base &= 0xfffffff0;
    phys_bytes vmem_size = x_res * y_res * 8;

    void* vmem_base = mm_map_phys(SELF, vmem_phys_base, vmem_size, 0);
    if (vmem_base == MAP_FAILED) return 0;

    fb_mem_base = vmem_base;
    fb_mem_size = vmem_size;

    return 1;
}
