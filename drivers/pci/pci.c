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
#include <errno.h>
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/service.h>
#include <libsysfs/libsysfs.h>

#include <asm/pci.h>
#include "pci.h"
#include "pci_dev_attr.h"

#define PCI_DEBUG

#ifdef PCI_DEBUG
#define DBGPRINT(x) printl(x)
#else
#define DBGPRINT(x)
#endif

struct pcibus pcibus[NR_PCIBUS];
static int nr_pcibus = 0;

struct pcidev pcidev[NR_PCIDEV];
static int nr_pcidev = 0;

static bus_type_id_t pci_bus_id;

static void pci_intel_init();
static void pci_probe_bus(int busind);

static int get_busind(int busnr)
{
    int i;
    for (i = 0; i < nr_pcibus; i++) {
        if (pcibus[i].busnr == busnr) return i;
    }

    panic("get_busind failed\n");

    return 1;
}

u8 pci_read_attr_u8(int devind, int port)
{
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].rreg_u8(busind, devind, port);
}

u16 pci_read_attr_u16(int devind, int port)
{
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].rreg_u16(busind, devind, port);
}

u32 pci_read_attr_u32(int devind, int port)
{
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].rreg_u32(busind, devind, port);
}

void pci_write_attr_u16(int devind, int port, u16 value)
{
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    pcibus[busind].wreg_u16(busind, devind, port, value);
}

void pci_write_attr_u32(int devind, int port, u32 value)
{
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    pcibus[busind].wreg_u32(busind, devind, port, value);
}

int pci_init()
{
    int retval;

    printl("pci: PCI driver is running.\n");

    retval = dm_bus_register("pci", &pci_bus_id);
    if (retval) return retval;

    pci_intel_init();

    int i;
    for (i = 0; i < NR_PRIV_PROCS; i++) {
        pci_acl[i].inuse = 0;
    }

    return 0;
}

static int pci_register_bus(int busind)
{
    struct device_info devinf;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "pci%02x", pcibus[busind].busnr);
    devinf.bus = NO_BUS_ID;
    devinf.class = NO_CLASS_ID;
    devinf.parent = NO_DEVICE_ID;

    return dm_device_register(&devinf, &pcibus[busind].dev_id);
}

static int pci_register_device(int devind)
{
    int busind = get_busind(pcidev[devind].busnr);
    int retval;
    device_id_t device_id;
    struct device_info devinf;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "pci%02x:%02x:%x",
             pcidev[devind].busnr, pcidev[devind].dev, pcidev[devind].func);
    devinf.bus = pci_bus_id;
    devinf.class = NO_CLASS_ID;
    devinf.parent = pcibus[busind].dev_id;
    devinf.devt = NO_DEV;

    retval = dm_device_register(&devinf, &device_id);
    if (retval) return retval;
    pcidev[devind].dev_id = device_id;

    struct device_attribute attr;
    dm_init_device_attr(&attr, device_id, "vendor", SF_PRIV_OVERWRITE,
                        (void*)&pcidev[devind], pci_vendor_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, device_id, "device", SF_PRIV_OVERWRITE,
                        (void*)&pcidev[devind], pci_device_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, device_id, "class", SF_PRIV_OVERWRITE,
                        (void*)&pcidev[devind], pci_class_show, NULL);
    dm_device_attr_add(&attr);

    return 0;
}

static void pci_intel_init()
{
    u32 bus, dev, func;
    int retval;

    bus = 0;
    dev = 0;
    func = 0;

    u16 vendor = pcii_read_u16(bus, dev, func, PCI_VID);
    u16 device = pcii_read_u16(bus, dev, func, PCI_DID);

    if (vendor == 0xffff && device == 0xffff) return;

    if (nr_pcibus >= NR_PCIBUS) return;

    int busind = nr_pcibus++;

    pcibus[busind].busnr = 0;
    retval = pci_register_bus(busind);
    if (retval) {
        nr_pcibus--;
        return;
    }

    pcibus[busind].rreg_u8 = pcii_rreg_u8;
    pcibus[busind].rreg_u16 = pcii_rreg_u16;
    pcibus[busind].rreg_u32 = pcii_rreg_u32;
    pcibus[busind].wreg_u16 = pcii_wreg_u16;
    pcibus[busind].wreg_u32 = pcii_wreg_u32;

    pci_probe_bus(busind);
}

static int record_bar(int devind, int bar_nr, int last)
{
    int reg, width, nr_bars, type;
    u32 bar, bar2;

    width = 1;
    reg = PCI_BAR + bar_nr * 4;

    bar = pci_read_attr_u32(devind, reg);

    if (bar & PCI_BAR_IO) {
        /* determine BAR's size */
        pci_write_attr_u32(devind, reg, 0xffffffff);
        bar2 = pci_read_attr_u32(devind, reg);

        pci_write_attr_u32(devind, reg, bar);

        bar &= PCI_BAR_IO_MASK;
        bar2 &= PCI_BAR_IO_MASK;
        bar2 = (~bar2 & 0xffff) + 1;

        nr_bars = pcidev[devind].nr_bars++;
        pcidev[devind].bars[nr_bars].base = bar;
        pcidev[devind].bars[nr_bars].size = bar2;
        pcidev[devind].bars[nr_bars].nr = bar_nr;
        pcidev[devind].bars[nr_bars].flags = PBF_IO;
    } else {
        type = (bar & PCI_BAR_TYPE);

        switch (type) {
        case PCI_TYPE_32:
        case PCI_TYPE_32_1M:
            break;

        case PCI_TYPE_64:
            if (last) {
                return width;
            }

            width++;
            bar2 = pci_read_attr_u32(devind, reg + 4);

            if (bar2) {
                return width;
            }
            break;

        default:
            return width;
        }

        pci_write_attr_u32(devind, reg, 0xffffffff);
        bar2 = pci_read_attr_u32(devind, reg);

        pci_write_attr_u32(devind, reg, bar);

        if (bar2 == 0) {
            return width;
        }

        bar &= PCI_BAR_MEM_MASK;
        bar2 &= PCI_BAR_MEM_MASK;
        bar2 = (~bar2 & 0xffff) + 1;

        nr_bars = pcidev[devind].nr_bars++;
        pcidev[devind].bars[nr_bars].base = bar;
        pcidev[devind].bars[nr_bars].size = bar2;
        pcidev[devind].bars[nr_bars].nr = bar_nr;
        pcidev[devind].bars[nr_bars].flags = 0;
    }

    return width;
}

static void record_bars(int devind, int last_reg)
{
    int i, reg, width;

    for (i = 0, reg = PCI_BAR; reg <= last_reg; i += width, reg += 4 * width) {
        width = record_bar(devind, i, reg == last_reg);
    }
}

static void pci_probe_bus(int busind)
{
    u8 bus_nr = pcibus[busind].busnr;
    int retval;
    int devind = nr_pcidev;

    int i = 0, func = 0;
    for (i = 0; i < 32; i++) {
        for (func = 0; func < 8; func++) {
            pcidev[devind].busnr = bus_nr;
            pcidev[devind].dev = i;
            pcidev[devind].func = func;

            u16 vendor = pci_read_attr_u16(devind, PCI_VID);
            u16 device = pci_read_attr_u16(devind, PCI_DID);
            u8 headt = pci_read_attr_u8(devind, PCI_HEADT);

            if (vendor == 0xffff) {
                if (func == 0) break;

                continue;
            }

            u8 baseclass = pci_read_attr_u8(devind, PCI_BCR);
            u8 subclass = pci_read_attr_u8(devind, PCI_SCR);
            u8 infclass = pci_read_attr_u8(devind, PCI_PIFR);

            devind = nr_pcidev;

            if ((retval = pci_register_device(devind)) != 0) continue;

            nr_pcidev++;

            pcidev[devind].vid = vendor;
            pcidev[devind].did = device;
            pcidev[devind].baseclass = baseclass;
            pcidev[devind].subclass = subclass;
            pcidev[devind].infclass = infclass;
            pcidev[devind].headt = headt;
            pcidev[devind].nr_bars = 0;

            char* name = pci_dev_name(vendor, device);
            if (name) {
                printl("pci %d.%02x.%x: (0x%04x:0x%04x) %s\n", bus_nr, i, func,
                       vendor, device, name);
            } else {
                printl("pci %d.%02x.%x: (0x%04x:0x%04x) Unknown device\n",
                       bus_nr, i, func, vendor, device);
            }

            switch (headt & PHT_MASK) {
            case PHT_NORMAL:
                record_bars(devind, PCI_BAR_6);

                break;
            default:
                printl("pci %d.%02x.%x: unknown header type: %d\n", bus_nr, i,
                       func, headt);
            }

            devind = nr_pcidev;
        }
    }
}

static int visible(struct pci_acl* acl, int devind)
{
    int i;

    if (!acl) return TRUE;

    for (i = 0; i < acl->nr_pci_id; i++) {
        if (acl->pci_id[i].vid == pcidev[devind].vid &&
            acl->pci_id[i].did == pcidev[devind].did) {
            return TRUE;
        }
    }

    if (!acl->nr_pci_class) return FALSE;

    u32 classid = (pcidev[devind].baseclass << 16) |
                  (pcidev[devind].subclass << 8) | pcidev[devind].infclass;
    for (i = 0; i < acl->nr_pci_class; i++) {
        if (acl->pci_class[i].classid == (classid & acl->pci_class[i].mask))
            return TRUE;
    }

    return FALSE;
}

int _pci_first_dev(struct pci_acl* acl, int* devind, u16* vid, u16* did,
                   device_id_t* dev_id)
{
    int i;

    for (i = 0; i < nr_pcidev; i++) {
        if (!visible(acl, i)) continue;
        break;
    }

    if (i >= nr_pcidev) return ESRCH;

    *devind = i;
    *vid = pcidev[i].vid;
    *did = pcidev[i].did;
    *dev_id = pcidev[i].dev_id;

    return 0;
}

int _pci_next_dev(struct pci_acl* acl, int* devind, u16* vid, u16* did,
                  device_id_t* dev_id)
{
    int i;

    for (i = *devind + 1; i < nr_pcidev; i++) {
        if (!visible(acl, i)) continue;
        break;
    }

    if (i >= nr_pcidev) return ESRCH;

    *devind = i;
    *vid = pcidev[i].vid;
    *did = pcidev[i].did;
    *dev_id = pcidev[i].dev_id;

    return 0;
}

int _pci_get_bar(int devind, int port, u32* base, u32* size, int* ioflag)
{
    int i, reg;

    if (devind < 0 || devind >= nr_pcidev) {
        return EINVAL;
    }

    for (i = 0; i < pcidev[devind].nr_bars; i++) {
        reg = PCI_BAR + 4 * pcidev[devind].bars[i].nr;

        if (reg == port) {
            *base = pcidev[devind].bars[i].base;
            *size = pcidev[devind].bars[i].size;
            *ioflag = !!(pcidev[devind].bars[i].flags & PBF_IO);

            return 0;
        }
    }

    return EINVAL;
}

static int pci_find_cap_start(int devind)
{
    u16 status = pci_read_attr_u8(devind, PCI_SR);

    if (!(status & PSR_CAPPTR)) {
        return 0;
    }

    switch (pcidev[devind].headt) {
    case PHT_NORMAL:
    case PHT_BRIDGE:
        return PCI_CAPPTR;
    }

    return 0;
}

static int pci_find_next_cap(int devind, u8 pos, int cap)
{
    u8 id;
    u16 ent;

    pos = pci_read_attr_u8(devind, pos);

    while (TRUE) {
        if (pos < 0x40) break;
        pos &= PCI_CP_MASK;
        ent = pci_read_attr_u16(devind, pos);

        id = ent & 0xff;
        if (id == 0xff) break;
        if (id == cap) return pos;
        pos = (ent >> 8);
    }

    return 0;
}

int _pci_find_capability(int devind, int cap)
{
    int pos;

    pos = pci_find_cap_start(devind);
    if (pos) pos = pci_find_next_cap(devind, pos, cap);

    return pos;
}

int _pci_find_next_capability(int devind, u8 pos, int cap)
{
    return pci_find_next_cap(devind, pos + PCI_CAP_LIST_NEXT, cap);
}
