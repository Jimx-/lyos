#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <libdevman/libdevman.h>
#include <libchardriver/libchardriver.h>

#include "libdrmdriver.h"
#include "proto.h"

static struct drm_device* drm_device;
static class_id_t drm_class_id = NO_CLASS_ID;

static const char* lib_name = "libdrmdriver";

static int drm_open(dev_t minor, int access);
static int drm_close(dev_t minor);
static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint,
                     mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id);
static int drm_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr);

static struct chardriver drm_chrdrv = {.cdr_open = drm_open,
                                       .cdr_close = drm_close,
                                       .cdr_ioctl = drm_ioctl,
                                       .cdr_mmap = drm_mmap};

static int drm_open(dev_t minor, int access)
{
    if (minor != drm_device->primary.index) {
        return ENXIO;
    }

    drm_gem_open(drm_device);

    return 0;
}

static int drm_close(dev_t minor) { return 0; }

static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint,
                     mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id)
{
    if (minor != drm_device->primary.index) {
        return ENXIO;
    }

    return drm_do_ioctl(drm_device, request, endpoint, grant, user_endpoint,
                        id);
}

static int drm_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr)
{
    if (minor != drm_device->primary.index) {
        return ENXIO;
    }

    return drm_gem_mmap(drm_device, endpoint, addr, offset, length, retaddr);
}

int drm_device_init(struct drm_device* dev, struct drm_driver* drv,
                    device_id_t parent)
{
    dev->driver = drv;
    dev->device_id = parent;

    dev->primary.index = 0;

    return 0;
}

int drmdriver_register_device(struct drm_device* dev)
{
    dev_t devt;
    struct device_info devinf;
    int retval;

    if (drm_class_id == NO_CLASS_ID) {
        retval = dm_class_get_or_create("drm", &drm_class_id);
        if (retval)
            panic("%s: %s failed to retrieve DRM class ID", lib_name,
                  __FUNCTION__);
    }

    /* add primary node */
    devt = MAKE_DEV(DEV_DRM, dev->primary.index);
    dm_cdev_add(devt);

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "card%d", dev->primary.index);
    snprintf(devinf.path, sizeof(devinf.path), "dri/card%d",
             dev->primary.index);
    devinf.bus = NO_BUS_ID;
    devinf.class = drm_class_id;
    devinf.parent = dev->device_id;
    devinf.devt = devt;
    devinf.type = DT_CHARDEV;

    retval = dm_device_register(&devinf, &dev->primary.device_id);

    return retval;
}

int drmdriver_task(struct drm_device* drm_dev)
{
    drm_device = drm_dev;

    return chardriver_task(&drm_chrdrv);
}
