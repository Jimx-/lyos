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
#include <errno.h>
#include <stddef.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <lyos/pci_utils.h>

#include "fb.h"
#include "arch_fb.h"

struct dev_probe {
    u16 vid;
    u16 did;
    fb_dev_init_func init_func;
} dev_probes[] = {
    {.vid = 0x1234, .did = 0x1111, fb_init_bochs},
    {.vid = 0xffff, .did = 0xffff, NULL},
};

void* fb_mem_vir;
phys_bytes fb_mem_phys;
size_t fb_mem_size;
static int initialized = 0;

int arch_init_fb(int minor)
{

    int devind;
    u16 vid, did;
    int retval = pci_first_dev(&devind, &vid, &did, NULL);
    int ok = 0;

    if (minor != 0) {
        return ENXIO;
    }

    if (initialized) {
        return 0;
    }

    initialized = 1;

    while (retval == 0) {
        struct dev_probe* probe = dev_probes;
        while (probe->vid != 0xffff) {
            if (probe->vid == vid && probe->did == did) {
                if (probe->init_func) {
                    if (probe->init_func(devind)) ok = 1;
                }
            }
            probe++;
        }

        retval = pci_next_dev(&devind, &vid, &did, NULL);
    }

    if (!ok) {
        panic("fb: no supported framebuffer devices");
    }

    return 0;
}

int arch_get_device(int minor, void** base, size_t* size)
{
    if (!initialized || minor != 0) {
        return ENXIO;
    }

    *base = fb_mem_vir;
    *size = fb_mem_size;
    return OK;
}

int arch_get_device_phys(int minor, phys_bytes* phys_base, phys_bytes* size)
{
    if (!initialized || minor != 0) {
        return ENXIO;
    }

    *phys_base = fb_mem_phys;
    *size = fb_mem_size;
    return OK;
}
