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
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <lyos/service.h>
#include <lyos/vm.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>
#include <sys/mman.h>

#include "fb.h"

static int init_fb();

static int fb_open(dev_t minor, int access, endpoint_t user_endpt);
static int fb_close(dev_t minor);
static ssize_t fb_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, int flags,
                       cdev_id_t id);
static ssize_t fb_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, int flags,
                        cdev_id_t id);
static int fb_ioctl(dev_t minor, int request, endpoint_t endpoint,
                    mgrant_id_t grant, int flags, endpoint_t user_endpoint,
                    cdev_id_t id);
static int fb_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                   size_t length, char** retaddr);

static int open_counter[NR_FB_DEVS];

static class_id_t fb_subsys_id;

static struct chardriver fbdriver = {
    .cdr_open = fb_open,
    .cdr_close = fb_close,
    .cdr_read = fb_read,
    .cdr_write = fb_write,
    .cdr_ioctl = fb_ioctl,
    .cdr_mmap = fb_mmap,
};

/*****************************************************************************
 *                              main
 *****************************************************************************/
/**
 * <Ring 3> The main loop of framebuffer driver.
 *
 *****************************************************************************/
int main()
{
    serv_register_init_fresh_callback(init_fb);
    serv_init();

    return chardriver_task(&fbdriver);
}

static int init_fb()
{
    int i;
    struct device_info devinf;
    device_id_t device_id;
    dev_t devt;
    int retval;

    printl("fb: framebuffer driver is running\n");

    retval = dm_class_register("fb", &fb_subsys_id);
    if (retval) panic("fb: cannot register framebuffer subsystem");

    for (i = 0; i < NR_FB_DEVS; i++) {
        open_counter[i] = 0;
        devt = MAKE_DEV(DEV_CHAR_FB, i);
        dm_cdev_add(devt);

        memset(&devinf, 0, sizeof(devinf));
        snprintf(devinf.name, sizeof(devinf.name), "fb%d", i);
        devinf.bus = NO_BUS_ID;
        devinf.class = fb_subsys_id;
        devinf.parent = NO_DEVICE_ID;
        devinf.devt = devt;
        devinf.type = DT_CHARDEV;

        retval = dm_device_register(&devinf, &device_id);
        if (retval) panic("fb: cannot register framebuffer device");
    }
    return 0;
}

static int fb_open(dev_t minor, int access, endpoint_t user_endpt)
{
    if (minor < 0 || minor >= NR_FB_DEVS) return ENXIO;

    int retval = arch_init_fb(minor);
    if (retval) {
        return ENXIO;
    }

    open_counter[minor]++;
    return OK;
}

static int fb_close(dev_t minor)
{
    if (minor < 0 || minor >= NR_FB_DEVS) return ENXIO;

    open_counter[minor]--;
    return OK;
}

static ssize_t fb_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, int flags,
                       cdev_id_t id)
{
    return 0;
}

static ssize_t fb_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, int flags,
                        cdev_id_t id)
{
    int retval = OK;
    void* base;
    size_t size;
    if (minor < 0 || minor >= NR_FB_DEVS) return -ENXIO;

    retval = arch_get_device(minor, &base, &size);
    if (retval != OK) return retval;

    if (count == 0 || pos >= size) return OK;
    if (pos + count > size) {
        count = size - pos;
    }

    safecopy_from(endpoint, grant, 0, (void*)(base + (size_t)pos), count);

    return count;
}

static int fb_ioctl(dev_t minor, int request, endpoint_t endpoint,
                    mgrant_id_t grant, int flags, endpoint_t user_endpoint,
                    cdev_id_t id)
{
    return 0;
}

static int fb_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                   size_t length, char** retaddr)
{
    int retval = OK;
    phys_bytes base, size;
    if (minor < 0 || minor >= NR_FB_DEVS) return ENXIO;

    retval = arch_get_device_phys(minor, &base, &size);
    if (retval != OK) return retval;

    if (length == 0 || offset >= size) return OK;
    if (offset + length > size) {
        length = size - offset;
    }

    char* mapped = mm_map_phys(endpoint, base + (size_t)offset, length, 0);
    if (mapped == MAP_FAILED) {
        return ENOMEM;
    }

    *retaddr = mapped;
    return 0;
}
