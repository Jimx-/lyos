#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>
#include <lyos/timer.h>

#include <libasyncdriver/libasyncdriver.h>
#include <libdevman/libdevman.h>

#include "proto.h"
#include "hcd.h"

#define MAX_THREADS 16

static bus_type_id_t usb_bus_id;

#if CONFIG_OF
void* boot_params;
#endif

static int usbd_process_on_thread(const MESSAGE* msg);
static void usbd_process(MESSAGE* msg);

static const struct asyncdriver asyncdriver = {
    .name = "usbd_async",

    .process_on_thread = usbd_process_on_thread,
    .process = usbd_process,
};

static int usbd_process_on_thread(const MESSAGE* msg)
{
    return msg->type != NOTIFY_MSG && msg->type != DM_REPLY &&
           msg->type != DM_DEVICE_ATTR_SHOW;
}

int usb_register_device(struct usb_device* udev)
{
    struct usb_bus* bus = udev->bus;
    device_id_t device_id;
    struct device_info devinf;
    dev_t devt;
    int retval;

    memset(&devinf, 0, sizeof(devinf));

    if (!udev->parent) {
        snprintf(devinf.name, sizeof(devinf.name), "usb%d", bus->busnum);
    } else {
        snprintf(devinf.name, sizeof(devinf.name), "%d-%s", bus->busnum,
                 udev->devpath);
    }

    devt = MAKE_DEV(USB_DEVICE_MAJOR,
                    (((udev->bus->busnum - 1) * 64) + (udev->devnum - 1)));
    dm_async_cdev_add(devt);

    devinf.bus = usb_bus_id;
    devinf.class = NO_CLASS_ID;
    devinf.parent = udev->parent ? udev->parent->dev_id : NO_DEVICE_ID;
    devinf.type = DT_CHARDEV;
    devinf.devt = devt;

    retval = dm_async_device_register(&devinf, &device_id);
    if (retval) return retval;

    udev->dev_id = device_id;

    retval = usb_create_sysfs_dev_files(udev);
    if (retval) return retval;

    return 0;
}

static int usbd_init(void)
{
    struct sysinfo* sysinfo;
    int retval;

    printl("usbd: USB daemon is running\n");

    get_sysinfo(&sysinfo);

    retval = dm_bus_register("usb", &usb_bus_id);
    if (retval) return retval;

#if CONFIG_OF
    boot_params = sysinfo->boot_params;
#endif

#if CONFIG_USB_PCI
    hcd_pci_init();
#endif

    return 0;
}

static void usbd_process(MESSAGE* msg)
{
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (src) {
        case INTERRUPT:
            usb_hcd_intr(msg->INTERRUPTS);
            break;
        }

        return;
    }

    switch (msg->type) {
    case DM_BUS_ATTR_SHOW:
    case DM_BUS_ATTR_STORE:
        msg->CNT = dm_bus_attr_handle(msg);
        break;
    case DM_DEVICE_ATTR_SHOW:
    case DM_DEVICE_ATTR_STORE:
        dm_device_attr_handle(msg);
        msg->RETVAL = SUSPEND;
        break;
    case DM_REPLY:
        dm_async_reply(msg);
        msg->RETVAL = SUSPEND;
        break;
    default:
        msg->RETVAL = ENOSYS;
        break;
    }

    if (msg->RETVAL != SUSPEND) {
        msg->type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, msg);
    }
}

static void usbd_post_init(void)
{
#if CONFIG_USB_PCI
    hcd_pci_scan();
#endif

#if CONFIG_USB_DWC2
    dwc2_scan();
#endif
}

int main()
{
    serv_register_init_fresh_callback(usbd_init);
    serv_init();

    asyncdrv_task(&asyncdriver, MAX_THREADS, usbd_post_init);

    return 0;
}
