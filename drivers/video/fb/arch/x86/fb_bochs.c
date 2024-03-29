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
#include <stdlib.h>
#include <string.h>
#include "lyos/const.h"
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/pci.h>

#include "fb.h"
#include "arch_fb.h"

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

/*
static u16 bochs_read(u16 reg)
{
    portio_outw(VBE_DISPI_IOPORT_INDEX, reg);

    u16 v;
    portio_inw(VBE_DISPI_IOPORT_DATA, &v);
    return v;
}

static void bochs_write(u16 reg, u16 val)
{
    portio_outw(VBE_DISPI_IOPORT_INDEX, reg);
    portio_outw(VBE_DISPI_IOPORT_DATA, val);
}
*/

void fb_init_bochs(int devind)
{
    struct fb_info* fb;
    struct fb_videomode* mode;
    int x_res = 1024;
    int y_res = 768;
    int ret;

    fb = malloc(sizeof(*fb));
    if (!fb) return;

    mode = malloc(sizeof(*mode));
    if (!mode) goto out_free_fb;

    memset(fb, 0, sizeof(*fb));
    memset(mode, 0, sizeof(*mode));

    phys_bytes vmem_phys_base = pci_attr_r32(devind, PCI_BAR);
    vmem_phys_base &= 0xfffffff0;
    phys_bytes vmem_size = x_res * y_res * 4;

    void* vmem_base = mm_map_phys(SELF, vmem_phys_base, vmem_size, 0);
    if (vmem_base == MAP_FAILED) goto out_free_mode;

    fb->screen_base = vmem_base;
    fb->screen_base_phys = vmem_phys_base;
    fb->screen_size = vmem_size;

    mode->xres = x_res;
    mode->yres = y_res;
    fb->mode = mode;

    ret = register_framebuffer(fb);
    if (!ret) return;

out_free_mode:
    free(mode);
out_free_fb:
    free(fb);
}
