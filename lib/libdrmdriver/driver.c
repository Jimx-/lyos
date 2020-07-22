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

static int primary_index = 0;
static device_id_t device_id = NO_DEVICE_ID;

struct drm_driver* drm_driver_tab;

static int drm_open(dev_t minor, int access);
static int drm_close(dev_t minor);
static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id);
static int drm_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr);

static struct chardriver drm_chrdrv = {.cdr_open = drm_open,
                                       .cdr_close = drm_close,
                                       .cdr_ioctl = drm_ioctl,
                                       .cdr_mmap = drm_mmap};

static int drm_open(dev_t minor, int access)
{
    if (minor != primary_index) {
        return ENXIO;
    }

    drm_gem_open();

    return 0;
}

static int drm_close(dev_t minor) { return 0; }

static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id)
{
    if (minor != primary_index) {
        return ENXIO;
    }

    return drm_do_ioctl(request, endpoint, buf, id);
}

static int drm_mmap(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr)
{
    if (minor != primary_index) {
        return ENXIO;
    }

    return drm_gem_mmap(endpoint, addr, offset, length, retaddr);
}

static int drm_register_device(device_id_t dev_id)
{
    dev_t devt;
    struct device_info devinf;
    int retval;

    /* add primary node */
    devt = MAKE_DEV(DEV_DRM, primary_index);
    dm_cdev_add(devt);

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "card%d", primary_index);
    devinf.bus = NO_BUS_ID;
    devinf.class = NO_CLASS_ID;
    devinf.parent = dev_id;
    devinf.devt = devt;
    devinf.type = DT_CHARDEV;

    retval = dm_device_register(&devinf, &device_id);

    return retval;
}

int drmdriver_task(struct drm_driver* drm_driver)
{
    int retval;

    retval = drm_register_device(drm_driver->device_id);
    if (retval) return retval;

    drm_driver_tab = drm_driver;

    return chardriver_task(&drm_chrdrv);
}
