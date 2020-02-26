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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/config.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/list.h>
#include <lyos/sysutils.h>
#include <lyos/service.h>
#include <lyos/vm.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>
#include <sys/mman.h>

#include "fb.h"

PRIVATE int init_fb();

PRIVATE int fb_open(dev_t minor, int access);
PRIVATE int fb_close(dev_t minor);
PRIVATE ssize_t fb_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                        unsigned int count, cdev_id_t id);
PRIVATE ssize_t fb_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                         unsigned int count, cdev_id_t id);
PRIVATE int fb_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id);
PRIVATE int fb_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr);

PRIVATE int open_counter[NR_FB_DEVS];

PRIVATE struct chardriver fbdriver = {
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
PUBLIC int main()
{
    serv_register_init_fresh_callback(init_fb);
    serv_init();

    return chardriver_task(&fbdriver);
}

PRIVATE int init_fb()
{
    printl("fb: framebuffer driver is running\n");

    int i;
    for (i = 0; i < NR_FB_DEVS; i++) {
        open_counter[i] = 0;
        dm_cdev_add(MAKE_DEV(DEV_CHAR_FB, i));
    }
    return 0;
}

PRIVATE int fb_open(dev_t minor, int access)
{
    if (minor < 0 || minor >= NR_FB_DEVS) return ENXIO;

    int retval = arch_init_fb(minor);
    if (retval) {
        return ENXIO;
    }

    open_counter[minor]++;
    return OK;
}

PRIVATE int fb_close(dev_t minor)
{
    if (minor < 0 || minor >= NR_FB_DEVS) return ENXIO;

    open_counter[minor]--;
    return OK;
}

PRIVATE ssize_t fb_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                        unsigned int count, cdev_id_t id)
{
    return 0;
}

PRIVATE ssize_t fb_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                         unsigned int count, cdev_id_t id)
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

    data_copy(SELF, (void*)(base + (size_t)pos), endpoint, buf, count);

    return count;
}

PRIVATE int fb_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id)
{
    return 0;
}

PRIVATE int fb_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
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

    char* mapped =
        mm_map_phys(endpoint, (void*)(base + (size_t)offset), length);
    if (mapped == MAP_FAILED) {
        return ENOMEM;
    }

    *retaddr = mapped;
    return 0;
}
