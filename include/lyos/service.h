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

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/bitmap.h>

#define NR_PCI_CLASS  4
#define NR_PCI_DEVICE 32

struct service_pci_id {
    u16 vid;
    u16 did;
};

struct service_pci_class {
    unsigned classid;
    unsigned mask;
};

struct service_up_req {
    char* cmdline;
    int cmdlen;
    char* progname;
    int prognamelen;
    char* label;
    int labellen;
    char* class;
    int classlen;

    int flags;
#define SUR_BASIC_SYSCALLS    0x01
#define SUR_ALLOW_PROXY_GRANT 0x02

    int nr_pci_id;
    struct service_pci_id pci_id[NR_PCI_DEVICE];
    int nr_pci_class;
    struct service_pci_class pci_class[NR_PCI_CLASS];

    bitchunk_t syscall_mask[BITCHUNKS(NR_SYS_CALLS)];

    int nr_domain;
    int domain[NR_DOMAIN];
};

struct pci_acl {
    endpoint_t endpoint;
    unsigned inuse;

    int nr_pci_id;
    struct service_pci_id pci_id[NR_PCI_DEVICE];
    int nr_pci_class;
    struct service_pci_class pci_class[NR_PCI_CLASS];
};

#define SERVICE_INIT_FRESH 0x1

typedef int (*serv_init_fresh_callback_t)();

void serv_register_init_fresh_callback(serv_init_fresh_callback_t cb);
int serv_init();

int pci_set_acl(struct pci_acl* pci_acl);

#endif
