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

#ifndef _PCI_H_
#define _PCI_H_

#include <libdevman/libdevman.h>

struct pcidev {
    u8 busnr;
    u8 devfn;

    u16 vid;
    u16 did;

    u8 baseclass;
    u8 subclass;
    u8 infclass;

    u8 headt;

    device_id_t dev_id;

    int nr_bars;
    struct {
        int nr;

        unsigned long base;
        size_t size;

        int flags;
    } bars[6];
};

#define PBF_IO 1

struct pcibus;

struct pci_ops {
    u8 (*rreg_u8)(struct pcibus* bus, unsigned int devfn, int where);
    u16 (*rreg_u16)(struct pcibus* bus, unsigned int devfn, int where);
    u32 (*rreg_u32)(struct pcibus* bus, unsigned int devfn, int where);
    void (*wreg_u8)(struct pcibus* bus, unsigned int devfn, int where,
                    u8 value);
    void (*wreg_u16)(struct pcibus* bus, unsigned int devfn, int where,
                     u16 value);
    void (*wreg_u32)(struct pcibus* bus, unsigned int devfn, int where,
                     u32 value);
};

struct pcibus {
    int busnr;
    device_id_t dev_id;

    int nr_resources;
    struct {
        int flags;
        unsigned long cpu_addr;
        unsigned long pci_addr;
        size_t size;
        unsigned long alloc_offset;
    } resources[10];

    u32 imask[4];
    struct {
        u32 child_intr[4];
        unsigned int irq_nr;
    } imap[16];

    const struct pci_ops* ops;
    void* private;
};

struct pci_device {
    int vendor;
    int device_id;
    char* name;
};

extern struct pci_acl pci_acl[];

#ifdef __i386__
void pci_intel_init();
#endif

#if CONFIG_OF
void pci_host_generic_init(void);
#endif

struct pcibus* pci_create_bus(int busnr, const struct pci_ops* ops,
                              void* private);
struct pcibus* pci_scan_bus(int busnr, const struct pci_ops* ops,
                            void* private);
void pci_probe_bus(struct pcibus* bus);

char* pci_dev_name(int vendor, int device_id);

int _pci_first_dev(struct pci_acl* acl, int* devind, u16* vid, u16* did,
                   device_id_t* dev_id);
int _pci_next_dev(struct pci_acl* acl, int* devind, u16* vid, u16* did,
                  device_id_t* dev_id);
int _pci_get_bar(int devind, int port, unsigned long* base, size_t* size,
                 int* ioflag);
int _pci_find_capability(int devind, int cap);
int _pci_find_next_capability(int devind, u8 pos, int cap);

u8 pci_read_attr_u8(int devind, int port);
u16 pci_read_attr_u16(int devind, int port);
u32 pci_read_attr_u32(int devind, int port);
void pci_write_attr_u8(int devind, int port, u8 value);
void pci_write_attr_u16(int devind, int port, u16 value);
void pci_write_attr_u32(int devind, int port, u32 value);

#endif
