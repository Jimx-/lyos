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
#include <stdlib.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <lyos/service.h>
#include <lyos/vm.h>
#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>
#include <sys/mman.h>
#include <lyos/idr.h>

#include "fb.h"

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

static class_id_t fb_subsys_id;

static struct idr fbdev_idr;

static struct chardriver fbdriver = {
    .cdr_open = fb_open,
    .cdr_close = fb_close,
    .cdr_read = fb_read,
    .cdr_write = fb_write,
    .cdr_ioctl = fb_ioctl,
    .cdr_mmap = fb_mmap,
};

static inline struct fb_info* fb_get_minor(int minor)
{
    return idr_find(&fbdev_idr, minor);
}

int register_framebuffer(struct fb_info* fb)
{
    struct device_info devinf;
    device_id_t device_id;
    dev_t devt;
    int index, ret;

    index = idr_alloc(&fbdev_idr, fb, 0, 0);
    if (index < 0) return -index;

    devt = MAKE_DEV(DEV_CHAR_FB, index);
    dm_cdev_add(devt);

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "fb%d", index);
    devinf.bus = NO_BUS_ID;
    devinf.class = fb_subsys_id;
    devinf.parent = NO_DEVICE_ID;
    devinf.devt = devt;
    devinf.type = DT_CHARDEV;

    ret = dm_device_register(&devinf, &device_id);
    if (ret) return ret;

    fb->dev_id = device_id;

    return 0;
}

static int register_kernel_fb(void)
{
    struct kinfo kinfo;
    void* fb_base;
    size_t screen_size;
    struct fb_info* fb;
    struct fb_videomode* mode;
    int ret = 0;

    get_kinfo(&kinfo);

    if (!kinfo.fb_base_phys) return ENXIO;

    fb = malloc(sizeof(*fb));
    if (!fb) return ENOMEM;

    ret = ENOMEM;
    mode = malloc(sizeof(*mode));
    if (!mode) goto out_free_fb;

    memset(fb, 0, sizeof(*fb));
    memset(mode, 0, sizeof(*mode));

    screen_size = kinfo.fb_screen_pitch * kinfo.fb_screen_height;
    fb_base = mm_map_phys(SELF, kinfo.fb_base_phys, screen_size, MMP_IO);
    if (fb_base == MAP_FAILED) goto out_free_mode;

    fb->screen_base = fb_base;
    fb->screen_base_phys = kinfo.fb_base_phys;
    fb->screen_size = screen_size;

    mode->xres = kinfo.fb_screen_width;
    mode->yres = kinfo.fb_screen_height;
    fb->mode = mode;

    ret = register_framebuffer(fb);
    if (ret) goto out_free_mode;

    return 0;

out_free_mode:
    free(mode);
out_free_fb:
    free(fb);
    return ret;
}

static int init_fb()
{
    int retval;

    printl("fb: framebuffer driver is running\n");

    idr_init(&fbdev_idr);

    retval = dm_class_register("fb", &fb_subsys_id);
    if (retval) panic("fb: cannot register framebuffer subsystem");

    register_kernel_fb();

    return 0;
}

static int fb_open(dev_t minor, int access, endpoint_t user_endpt)
{
    struct fb_info* fb = fb_get_minor(minor);
    if (!fb) return ENXIO;
    return OK;
}

static int fb_close(dev_t minor)
{
    struct fb_info* fb = fb_get_minor(minor);
    if (!fb) return ENXIO;
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
    struct fb_info* fb = fb_get_minor(minor);

    if (!fb) return -ENXIO;

    if (count == 0 || pos >= fb->screen_size) return 0;
    if (pos + count > fb->screen_size) {
        count = fb->screen_size - pos;
    }

    safecopy_from(endpoint, grant, 0, (void*)(fb->screen_base + (size_t)pos),
                  count);

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
    struct fb_info* fb = fb_get_minor(minor);
    char* mapped;

    if (!fb) return ENXIO;

    if (length == 0 || offset >= fb->screen_size) return 0;
    if (offset + length > fb->screen_size) {
        length = fb->screen_size - offset;
    }

    mapped =
        mm_map_phys(endpoint, fb->screen_base_phys + (size_t)offset, length, 0);
    if (mapped == MAP_FAILED) {
        return ENOMEM;
    }

    *retaddr = mapped;
    return 0;
}

int main()
{
    serv_register_init_fresh_callback(init_fb);
    serv_init();

    return chardriver_task(&fbdriver);
}
