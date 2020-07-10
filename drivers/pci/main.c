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

#include <asm/pci.h>
#include "pci.h"

#define PCI_DEBUG

#ifdef PCI_DEBUG
#define DBGPRINT(x) printl(x)
#else
#define DBGPRINT(x)
#endif

int pci_init();

struct pci_acl pci_acl[NR_PRIV_PROCS];

static int do_set_acl(MESSAGE* m);
static int do_first_dev(MESSAGE* m);
static int do_next_dev(MESSAGE* m);
static int do_attr_r8(MESSAGE* m);
static int do_attr_r16(MESSAGE* m);
static int do_attr_r32(MESSAGE* m);
static int do_attr_w16(MESSAGE* m);
static int do_get_bar(MESSAGE* m);

int main()
{
    // serv_register_init_fresh_callback(pci_init);
    // serv_init();
    pci_init();

    MESSAGE msg;

    while (TRUE) {

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        int msgtype = msg.type;

        switch (msgtype) {
        case PCI_SET_ACL:
            msg.RETVAL = do_set_acl(&msg);
            break;
        case PCI_FIRST_DEV:
            msg.RETVAL = do_first_dev(&msg);
            break;
        case PCI_NEXT_DEV:
            msg.RETVAL = do_next_dev(&msg);
            break;
        case PCI_ATTR_R8:
            msg.RETVAL = do_attr_r8(&msg);
            break;
        case PCI_ATTR_R16:
            msg.RETVAL = do_attr_r16(&msg);
            break;
        case PCI_ATTR_R32:
            msg.RETVAL = do_attr_r32(&msg);
            break;
        case PCI_ATTR_W16:
            msg.RETVAL = do_attr_w16(&msg);
            break;
        case PCI_GET_BAR:
            msg.RETVAL = do_get_bar(&msg);
            break;
        case DM_BUS_ATTR_SHOW:
        case DM_BUS_ATTR_STORE:
            msg.CNT = dm_bus_attr_handle(&msg);
            break;
        case DM_DEVICE_ATTR_SHOW:
        case DM_DEVICE_ATTR_STORE:
            msg.CNT = dm_device_attr_handle(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        if (msg.RETVAL != SUSPEND) {
            msg.type = SYSCALL_RET;
            send_recv(SEND_NONBLOCK, src, &msg);
        }
    }

    return 0;
}

static int do_set_acl(MESSAGE* m)
{
    int i;

    if (m->source != TASK_SERVMAN) return EPERM;

    for (i = 0; i < NR_PRIV_PROCS; i++) {
        if (!pci_acl[i].inuse) break;
    }

    if (i >= NR_PRIV_PROCS) return ENOMEM;

    int retval =
        data_copy(SELF, &pci_acl[i], m->source, m->BUF, sizeof(struct pci_acl));

    if (retval) return retval;

    pci_acl[i].inuse = 1;
    return 0;
}

static struct pci_acl* get_acl(endpoint_t ep)
{
    int i;
    for (i = 0; i < NR_PRIV_PROCS; i++) {
        if (!pci_acl[i].inuse) continue;

        if (pci_acl[i].endpoint == ep) return &pci_acl[i];
    }

    return NULL;
}

static int do_first_dev(MESSAGE* m)
{
    struct pci_acl* acl = get_acl(m->source);

    int devind;
    u16 vid, did;
    device_id_t dev_id;

    int retval = _pci_first_dev(acl, &devind, &vid, &did, &dev_id);

    if (retval) return retval;

    m->u.m3.m3i2 = devind;
    m->u.m3.m3i3 = vid;
    m->u.m3.m3i4 = did;
    m->u.m3.m3l1 = dev_id;

    return 0;
}

static int do_next_dev(MESSAGE* m)
{
    struct pci_acl* acl = get_acl(m->source);

    int devind = m->u.m3.m3i2;
    u16 vid, did;
    device_id_t dev_id;

    int retval = _pci_next_dev(acl, &devind, &vid, &did, &dev_id);

    if (retval) return retval;

    m->u.m3.m3i2 = devind;
    m->u.m3.m3i3 = vid;
    m->u.m3.m3i4 = did;
    m->u.m3.m3l1 = dev_id;

    return 0;
}

static int do_get_bar(MESSAGE* m)
{
    int devind = m->u.m3.m3i2;
    int port = m->u.m3.m3i3;

    u32 base, size;
    int ioflag;

    int retval = _pci_get_bar(devind, port, &base, &size, &ioflag);

    if (retval) return retval;

    m->u.m3.m3i2 = base;
    m->u.m3.m3i3 = size;
    m->u.m3.m3i4 = ioflag;

    return 0;
}

static int do_attr_r8(MESSAGE* m)
{
    // struct pci_acl * acl = get_acl(m->source);

    int devind = m->u.m3.m3i2;
    u16 offset = (u16)m->u.m3.m3i3;

    m->u.m3.m3i2 = pci_read_attr_u8(devind, offset);

    return 0;
}

static int do_attr_r16(MESSAGE* m)
{
    // struct pci_acl * acl = get_acl(m->source);

    int devind = m->u.m3.m3i2;
    u16 offset = (u16)m->u.m3.m3i3;

    m->u.m3.m3i2 = pci_read_attr_u16(devind, offset);

    return 0;
}

static int do_attr_r32(MESSAGE* m)
{
    // struct pci_acl * acl = get_acl(m->source);

    int devind = m->u.m3.m3i2;
    u16 offset = (u16)m->u.m3.m3i3;

    m->u.m3.m3i2 = pci_read_attr_u32(devind, offset);

    return 0;
}

static int do_attr_w16(MESSAGE* m)
{
    // struct pci_acl * acl = get_acl(m->source);

    int devind = m->u.m3.m3i2;
    u16 offset = (u16)m->u.m3.m3i3;
    u16 value = (u16)m->u.m3.m3i4;

    pci_write_attr_u16(devind, offset, value);

    return 0;
}
