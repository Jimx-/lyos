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

static int primary_index = 0;
static device_id_t device_id = NO_DEVICE_ID;

static int drm_open(dev_t minor, int access);
static int drm_close(dev_t minor);
static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id);

static struct chardriver drm_chrdrv = {
    .cdr_open = drm_open, .cdr_close = drm_close, .cdr_ioctl = drm_ioctl};

static int drm_open(dev_t minor, int access) { return 0; }

static int drm_close(dev_t minor) { return 0; }

static int drm_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                     cdev_id_t id)
{
    return ENOSYS;
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

int drmdriver_task(device_id_t dev_id, struct drm_driver* drm_driver)
{
    int retval;

    retval = drm_register_device(dev_id);
    if (retval) return retval;

    return chardriver_task(&drm_chrdrv);
}
