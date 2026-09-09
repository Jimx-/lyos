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
#include "stdio.h"
#include <stdlib.h>
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/irqctl.h>
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <libsysfs/libsysfs.h>
#include <uapi/linux/pci.h>

#include <asm/pci.h>
#if defined(__i386__) || defined(__x86_64__)
#include <asm/const.h>
#include <asm/protect.h>
#endif
#include "pci.h"
#include "pci_dev_attr.h"

#define PCI_MSIX_FLAGS          2
#define PCI_MSIX_FLAGS_ENABLE   0x8000
#define PCI_MSIX_FLAGS_MASKALL  0x4000
#define PCI_MSIX_TABLE          4
#define PCI_MSIX_TABLE_BIR      0x00000007
#define PCI_MSIX_TABLE_OFFSET   0xfffffff8

#define PCI_DEBUG

#ifdef PCI_DEBUG
#define DBGPRINT(x) printl(x)
#else
#define DBGPRINT(x)
#endif

static struct pcibus pcibus[NR_PCIBUS];
static int nr_pcibus = 0;

static struct pcidev pcidev[NR_PCIDEV];
static int nr_pcidev = 0;

static bus_type_id_t pci_bus_id;

#if CONFIG_OF
void* boot_params;
#endif

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
    struct pcidev* dev = &pcidev[devind];
    int busnr = dev->busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].ops->rreg_u8(&pcibus[busind], dev->devfn, port);
}

u16 pci_read_attr_u16(int devind, int port)
{
    struct pcidev* dev = &pcidev[devind];
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].ops->rreg_u16(&pcibus[busind], dev->devfn, port);
}

u32 pci_read_attr_u32(int devind, int port)
{
    struct pcidev* dev = &pcidev[devind];
    int busnr = pcidev[devind].busnr;
    int busind = get_busind(busnr);
    return pcibus[busind].ops->rreg_u32(&pcibus[busind], dev->devfn, port);
}

void pci_write_attr_u8(int devind, int port, u8 value)
{
    struct pcidev* dev = &pcidev[devind];
    int busnr = dev->busnr;
    int busind = get_busind(busnr);
    pcibus[busind].ops->wreg_u8(&pcibus[busind], dev->devfn, port, value);
}

void pci_write_attr_u16(int devind, int port, u16 value)
{
    struct pcidev* dev = &pcidev[devind];
    int busnr = dev->busnr;
    int busind = get_busind(busnr);
    pcibus[busind].ops->wreg_u16(&pcibus[busind], dev->devfn, port, value);
}

void pci_write_attr_u32(int devind, int port, u32 value)
{
    struct pcidev* dev = &pcidev[devind];
    int busnr = dev->busnr;
    int busind = get_busind(busnr);
    pcibus[busind].ops->wreg_u32(&pcibus[busind], dev->devfn, port, value);
}

int pci_init()
{
    int retval;
    struct sysinfo* sysinfo;

    printl("pci: PCI driver is running.\n");

    get_sysinfo(&sysinfo);

    retval = dm_bus_register("pci", &pci_bus_id);
    if (retval) return retval;

#if CONFIG_OF
    boot_params = sysinfo->boot_params;
    pci_host_generic_init();
#endif

#if defined(__i386__) || defined(__x86_64__)
#if CONFIG_ACPI
    if (!pci_acpi_init())
#endif
        pci_intel_init();
#endif

    int i;
    for (i = 0; i < NR_PRIV_PROCS; i++) {
        pci_acl[i].inuse = 0;
    }

    return 0;
}

static int pci_register_bus(int busind)
{
    struct device_info devinf;
    int retval;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "pci%02x", pcibus[busind].busnr);
    devinf.bus = NO_BUS_ID;
    devinf.class = NO_CLASS_ID;
    devinf.parent = NO_DEVICE_ID;

    retval = dm_device_register(&devinf, &pcibus[busind].dev_id);
    if (retval) return retval;

    return dm_device_publish(pcibus[busind].dev_id);
}

static int pci_register_device(int devind)
{
    int busind = get_busind(pcidev[devind].busnr);
    int retval;
    device_id_t device_id;
    struct device_info devinf;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "pci%02x:%02x:%x",
             pcidev[devind].busnr, PCI_SLOT(pcidev[devind].devfn),
             PCI_FUNC(pcidev[devind].devfn));
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

    return dm_device_publish(device_id);
}

struct pcibus* pci_create_bus(int busnr, const struct pci_ops* ops,
                              void* private)
{
    int busind;
    struct pcibus* bus;
    int retval;

    if (nr_pcibus >= NR_PCIBUS) return NULL;

    busind = nr_pcibus++;
    bus = &pcibus[busind];

    bus->busnr = busnr;

    retval = pci_register_bus(busind);
    if (retval != 0) {
        nr_pcibus--;
        return NULL;
    }

    bus->ops = ops;
    bus->private = private;

    return bus;
}

struct pcibus* pci_scan_bus(int busnr, const struct pci_ops* ops, void* private)
{
    struct pcibus* b;

    b = pci_create_bus(busnr, ops, private);
    pci_probe_bus(b);

    return b;
}

static int allocate_bar(struct pcibus* bus, int flags, size_t size,
                        unsigned long* pci_base, unsigned long* host_base)
{
    int i;

    for (i = 0; i < bus->nr_resources; i++) {
        if ((bus->resources[i].flags & PBF_IO) == (flags & PBF_IO)) {
            unsigned long offset = bus->resources[i].alloc_offset;

            offset = (offset + size - 1) & ~(size - 1);
            if (offset + size > bus->resources[i].size) continue;

            bus->resources[i].alloc_offset = offset + size;

            *pci_base = bus->resources[i].pci_addr + offset;
            *host_base = bus->resources[i].cpu_addr + offset;

            return 0;
        }
    }

    return ENOMEM;
}

static int record_bar(int devind, int bar_nr, int last)
{
    int reg, width, nr_bars, type;
    u32 bar, bar2, mask, mask2;
    unsigned long base;
    size_t size;
    int flags = 0;

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

        base = bar;
        size = bar2;
        flags = PBF_IO;
    } else {
        type = (bar & PCI_BAR_TYPE);

        switch (type) {
        case PCI_TYPE_32:
        case PCI_TYPE_32_1M:
            base = bar & PCI_BAR_MEM_MASK;

            pci_write_attr_u32(devind, reg, 0xffffffff);
            bar2 = pci_read_attr_u32(devind, reg);
            pci_write_attr_u32(devind, reg, bar);

            bar2 &= PCI_BAR_MEM_MASK;
            if (!bar2) return width;

            size = (~bar2 & 0xffff) + 1;

            break;

        case PCI_TYPE_64:
            if (last) {
                return width;
            }

            width++;
            bar2 = pci_read_attr_u32(devind, reg + 4);
            base = ((u64)bar2 << 32) | (bar & PCI_BAR_MEM_MASK);

            pci_write_attr_u32(devind, reg, 0xffffffff);
            pci_write_attr_u32(devind, reg + 4, 0xffffffff);
            mask = pci_read_attr_u32(devind, reg);
            mask2 = pci_read_attr_u32(devind, reg + 4);
            pci_write_attr_u32(devind, reg, bar);
            pci_write_attr_u32(devind, reg + 4, bar2);

            size = ((u64)mask2 << 32) | (mask & PCI_BAR_MEM_MASK);
            size = ~size + 1;

            break;

        default:
            return width;
        }
    }

    nr_bars = pcidev[devind].nr_bars++;
    pcidev[devind].bars[nr_bars].base = base;
    pcidev[devind].bars[nr_bars].size = size;
    pcidev[devind].bars[nr_bars].nr = bar_nr;
    pcidev[devind].bars[nr_bars].flags = flags;

    if (!base) {
        struct pcidev* dev = &pcidev[devind];
        struct pcibus* bus = &pcibus[get_busind(dev->busnr)];
        unsigned long pci_base = 0, host_base = 0;
        u16 cmd;

        allocate_bar(bus, flags, size, &pci_base, &host_base);
        pci_write_attr_u32(devind, reg, (u32)pci_base);
        if (width > 1)
            pci_write_attr_u32(devind, reg + 4, (u32)((u64)pci_base >> 32));
        pci_read_attr_u32(devind, reg);

        cmd = pci_read_attr_u16(devind, PCI_CR);
        if (flags & PBF_IO)
            cmd |= PCI_CR_IO_EN;
        else
            cmd |= PCI_CR_MEM_EN;
        pci_write_attr_u16(devind, PCI_CR, cmd);

        dev->bars[nr_bars].base = host_base;
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

static void record_irq(int devind)
{
    int ilr, ipr;

    ilr = pci_read_attr_u8(devind, PCI_ILR);
    ipr = pci_read_attr_u8(devind, PCI_IPR);

    if (ipr && !ilr) {
        struct pcidev* dev = &pcidev[devind];
        struct pcibus* bus = &pcibus[get_busind(dev->busnr)];
        unsigned int laddr[4];

        laddr[0] = dev->devfn << 8;
        laddr[1] = laddr[2] = 0;
        laddr[3] = ipr;

        int i, j;
        for (i = 0; i < 16; i++) {
            int match = 1;

            for (j = 0; j < 4; j++)
                if ((laddr[j] & bus->imask[j]) != bus->imap[i].child_intr[j]) {
                    match = 0;
                    break;
                }

            if (match) {
                ilr = bus->imap[i].irq_nr;
                break;
            }
        }

        pci_write_attr_u8(devind, PCI_ILR, ilr);
    }
}

void pci_probe_bus(struct pcibus* bus)
{
    u8 bus_nr = bus->busnr;
    int retval;
    int devind = nr_pcidev;

    int i = 0, func = 0;
    for (i = 0; i < 32; i++) {
        for (func = 0; func < 8; func++) {
            pcidev[devind].busnr = bus_nr;
            pcidev[devind].devfn = PCI_DEVFN(i, func);

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

            record_irq(devind);

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

int _pci_get_bar(int devind, int port, unsigned long* base, size_t* size,
                 int* ioflag)
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

static void pci_disable_intx(int devind)
{
#ifdef PCI_CR_INT_DIS
    u16 cmd = pci_read_attr_u16(devind, PCI_CR);

    pci_write_attr_u16(devind, PCI_CR, cmd | PCI_CR_INT_DIS);
#endif
}

static int pci_alloc_msi_irqs(int nr_irqs)
{
#if defined(__i386__) || defined(__x86_64__)
    return irqctl_alloc_msi(NR_IRQS_LEGACY, nr_irqs);
#else
    return -ENOSYS;
#endif
}

static int pci_setup_msi(int devind, int pos, int* irqs)
{
#if defined(__i386__) || defined(__x86_64__)
    int allocated_irq;
    u16 ctrl;
    u16 data;

    allocated_irq = pci_alloc_msi_irqs(1);
    if (allocated_irq < 0) return allocated_irq;

    ctrl = pci_read_attr_u16(devind, pos + PCI_MSI_FLAGS);
    pci_write_attr_u32(devind, pos + PCI_MSI_ADDRESS_LO, MSI_ADDR_BASE_LO);
    if (ctrl & PCI_MSI_FLAGS_64BIT) {
        pci_write_attr_u32(devind, pos + PCI_MSI_ADDRESS_HI, 0);
    }

    data = INT_VECTOR_IRQ0 + allocated_irq;
    if (ctrl & PCI_MSI_FLAGS_64BIT) {
        pci_write_attr_u16(devind, pos + PCI_MSI_DATA_64, data);
    } else {
        pci_write_attr_u16(devind, pos + PCI_MSI_DATA_32, data);
    }

    ctrl |= PCI_MSI_FLAGS_ENABLE;
    pci_write_attr_u16(devind, pos + PCI_MSI_FLAGS, ctrl);
    pci_disable_intx(devind);

    irqs[0] = allocated_irq;
    return 1;
#else
    return -ENOSYS;
#endif
}

static int pci_setup_msix(int devind, int pos, int min_vecs, int max_vecs,
                          int* irqs)
{
#if defined(__i386__) || defined(__x86_64__)
    unsigned long bar_base;
    size_t bar_size;
    int ioflag;
    u32 table;
    u32 table_off;
    int bir;
    volatile u32* msix_tbl;
    void* table_map;
    int first_irq;
    int nvec;
    u16 ctrl;
    int retval;
    int i;

    ctrl = pci_read_attr_u16(devind, pos + PCI_MSIX_FLAGS);
    nvec = (ctrl & 0x07ff) + 1;
    if (nvec > max_vecs) nvec = max_vecs;
    if (nvec < min_vecs) return -ENOSPC;

    table = pci_read_attr_u32(devind, pos + PCI_MSIX_TABLE);
    bir = table & PCI_MSIX_TABLE_BIR;
    table_off = table & PCI_MSIX_TABLE_OFFSET;

    retval = _pci_get_bar(devind, PCI_BAR + bir * 4, &bar_base, &bar_size,
                          &ioflag);
    if (retval) return -retval;
    if (ioflag || table_off + 16 * nvec > bar_size) return -EINVAL;

    first_irq = pci_alloc_msi_irqs(nvec);
    if (first_irq < 0) return first_irq;

    table_map = mm_map_phys(SELF, bar_base, bar_size, MMP_IO);
    if (!table_map) return -ENOMEM;

    msix_tbl = (volatile u32*)((char*)table_map + table_off);
    for (i = 0; i < nvec; i++) {
        volatile u32* entry = msix_tbl + i * 4;
        int irq = first_irq + i;

        entry[0] = MSI_ADDR_BASE_LO;
        entry[1] = 0;
        entry[2] = INT_VECTOR_IRQ0 + irq;
        entry[3] = 0;
        irqs[i] = irq;
    }

    ctrl |= PCI_MSIX_FLAGS_ENABLE;
    ctrl &= ~PCI_MSIX_FLAGS_MASKALL;
    pci_write_attr_u16(devind, pos + PCI_MSIX_FLAGS, ctrl);
    pci_disable_intx(devind);

    munmap(table_map, bar_size);

    return nvec;
#else
    return -ENOSYS;
#endif
}

int _pci_alloc_irq(int devind, int flags, int* irq)
{
    int retval;

    retval = _pci_alloc_irq_vectors(devind, flags, 1, 1, irq);
    return retval < 0 ? -retval : 0;
}

int _pci_alloc_irq_vectors(int devind, int flags, int min_vecs, int max_vecs,
                           int* irqs)
{
    int retval;
    int pos;

    if (devind < 0 || devind >= nr_pcidev || !irqs) return -EINVAL;
    if (min_vecs <= 0 || max_vecs < min_vecs) return -EINVAL;

    if (!flags) flags = PCI_IRQ_MSIX | PCI_IRQ_MSI;

    if (flags & PCI_IRQ_MSIX) {
        pos = _pci_find_capability(devind, PCI_CAP_ID_MSIX);
        if (pos > 0) {
            retval = pci_setup_msix(devind, pos, min_vecs, max_vecs, irqs);
            if (retval >= 0) return retval;
        }
    }

    if ((flags & PCI_IRQ_MSI) && min_vecs <= 1) {
        pos = _pci_find_capability(devind, PCI_CAP_ID_MSI);
        if (pos > 0) {
            retval = pci_setup_msi(devind, pos, irqs);
            if (retval >= 0) return retval;
        }
    }

    return -ENOSYS;
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
