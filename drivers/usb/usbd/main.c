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
#include <libusb/libusb.h>

#include "proto.h"
#include "hcd.h"

#define MAX_THREADS 16
#define MAX_DRIVERS 256

static bus_type_id_t usb_bus_id;

static struct async_work probe_work;

static DEF_LIST(usb_intfs);

struct usbd_driver {
    endpoint_t endpoint;
    struct usb_interface* intf;
    int status;
#define DRIVER_UNUSED 0
#define DRIVER_ACTIVE 1
#define DRIVER_BOUND  2
    unsigned int urb_id;
};

static struct usbd_driver usbd_drivers[MAX_DRIVERS];

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
    return msg->type != NOTIFY_MSG;
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

    return dm_async_device_publish(device_id);
}

int usb_register_interface(struct usb_interface* intf, int configuration,
                           int ifnum)
{
    struct usb_device* udev = intf->parent;
    device_id_t device_id;
    struct device_info devinf;
    int retval;

    memset(&devinf, 0, sizeof(devinf));

    snprintf(devinf.name, sizeof(devinf.name), "%d-%s:%d.%d", udev->bus->busnum,
             udev->devpath, configuration, ifnum);

    devinf.bus = usb_bus_id;
    devinf.class = NO_CLASS_ID;
    devinf.parent = udev->dev_id;
    devinf.devt = NO_DEV;

    retval = dm_async_device_register(&devinf, &device_id);
    if (retval) return retval;

    intf->dev_id = device_id;

    retval = usb_create_sysfs_intf_files(intf);
    if (retval) return retval;

    /* Make the interface bindable before udev can start its driver. */
    usb_probe_interface(intf);
    return dm_async_device_publish(device_id);
}

static struct usb_interface* find_interface(device_id_t dev_id)
{
    struct usb_interface* intf;
    list_for_each_entry(intf, &usb_intfs, list)
    {
        if (intf->dev_id == dev_id) return intf;
    }
    return NULL;
}

static struct usbd_driver* find_driver(endpoint_t endpoint)
{
    int i;
    for (i = 0; i < MAX_DRIVERS; i++) {
        if (usbd_drivers[i].status != DRIVER_UNUSED &&
            usbd_drivers[i].endpoint == endpoint) {
            return &usbd_drivers[i];
        }
    }
    return NULL;
}

static void do_register_driver(MESSAGE* msg)
{
    endpoint_t ep = msg->source;
    struct usbd_driver* drv;

    drv = find_driver(ep);

    msg->type = USB_REPLY;
    msg->u.m_usb_reply.status = 0;
    if (drv == NULL) msg->u.m_usb_reply.status = EPERM;
    send_recv(SEND, ep, msg);

    if (drv == NULL) return;

    msg->type = USB_DEVICE_CONNECT;
    msg->DEVICE = drv->intf->dev_id;
    msg->MASK = 1 << drv->intf->cur_altsetting->desc.bInterfaceNumber;
    asyncsend3(ep, msg, 0);
}

static int do_bind_device(MESSAGE* msg)
{
    endpoint_t ep = msg->PROC_NR;
    device_id_t dev_id = msg->DEVICE;
    struct usbd_driver* drv = NULL;
    int i;

    struct usb_interface* intf = find_interface(dev_id);
    if (intf == NULL) return ENODEV;

    for (i = 0; i < MAX_DRIVERS; i++) {
        if (usbd_drivers[i].status != DRIVER_UNUSED &&
            usbd_drivers[i].intf->dev_id == dev_id) {
            return EBUSY;
        }
    }

    for (i = 0; i < MAX_DRIVERS; i++) {
        if (usbd_drivers[i].status == DRIVER_UNUSED) {
            drv = &usbd_drivers[i];
            break;
        }
    }

    if (drv == NULL) return ENOMEM;

    drv->status = DRIVER_BOUND;
    drv->intf = intf;
    drv->endpoint = ep;
    drv->urb_id = 0;

    return 0;
}

static struct urb* libusb_urb_to_urb(struct usb_urb* usb_urb)
{
    struct urb* urb;

    urb = usb_alloc_urb(usb_urb->number_of_packets);
    if (urb == NULL) return urb;

    urb->interval = usb_urb->interval;

    if (usb_urb->type == USB_TRANSFER_CTL) {
        urb->setup_packet = usb_urb->setup_packet;
    }

    urb->transfer_buffer = usb_urb->buffer;
    urb->transfer_buffer_length = usb_urb->size;

    if (usb_urb->type == USB_TRANSFER_ISO) {
        urb->num_packets = usb_urb->number_of_packets;
        memcpy(urb->iso_desc, usb_urb->buffer + usb_urb->iso_desc_offset,
               urb->num_packets * sizeof(struct usb_iso_packet_descriptor));
    }

    return urb;
}

static void send_urb_reply(endpoint_t endpoint, int status, unsigned int urb_id)
{
    MESSAGE msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = USB_REPLY;
    msg.u.m_usb_reply.status = status;
    msg.u.m_usb_reply.urb_id = urb_id;
    send_recv(SEND, endpoint, &msg);
}

static void do_send_urb(MESSAGE* msg)
{
    endpoint_t src = msg->source;
    mgrant_id_t grant = msg->u.m_usb_send_urb.grant;
    size_t grant_size = msg->u.m_usb_send_urb.grant_size;
    struct usb_urb* usb_urb;
    struct urb* urb = NULL;
    struct usbd_driver* drv;
    struct usb_interface* intf;
    struct usb_device* dev = NULL;
    int length;
    int retval = 0;

    drv = find_driver(src);
    if (drv == NULL) {
        return;
    }

    usb_urb = (struct usb_urb*)malloc(grant_size + sizeof(void*));
    if (usb_urb == NULL) {
        retval = ENOMEM;
        goto err;
    }

    retval = safecopy_from(src, grant, 0, &usb_urb->dev_id, grant_size);
    if (retval) {
        goto err;
    }

    intf = find_interface(usb_urb->dev_id);
    if (intf == NULL) {
        retval = ENODEV;
        goto err;
    }

    dev = intf->parent;
    usb_get_dev(dev);

    urb = libusb_urb_to_urb(usb_urb);
    if (urb == NULL) {
        retval = ENOMEM;
        goto err;
    }

    usb_urb->urb_id = drv->urb_id++;
    send_urb_reply(src, retval, usb_urb->urb_id);

    urb->dev = dev;
    urb->pipe = (usb_urb->type << 30) | __create_pipe(dev, usb_urb->endpoint) |
                (usb_urb->direction == USB_IN ? USB_DIR_IN : USB_DIR_OUT);

    retval = usb_start_wait_urb(urb, &length);
    urb = NULL;

    usb_urb->status = retval;
    usb_urb->actual_length = length;

    retval = safecopy_to(src, grant, 0, &usb_urb->dev_id, grant_size);

    msg->type = USB_RQ_COMPLETE_URB;
    msg->u.m_usb_reply.status = retval;
    msg->u.m_usb_reply.urb_id = usb_urb->urb_id;
    asyncsend3(src, msg, 0);

    goto free;

err:
    send_urb_reply(src, retval, 0);

free:
    if (urb != NULL) {
        usb_free_urb(urb);
    }

    usb_put_dev(dev);

    if (usb_urb != NULL) {
        free(usb_urb);
    }
}

static int usbd_probe_interface(struct usb_interface* intf,
                                const struct usb_device_id* id)
{
    list_add(&intf->list, &usb_intfs);
    return 0;
}

static const struct usb_device_id usbd_id_table[] = {
    {.match_flags = 0, .bInterfaceClass = 1},
    {},
};

static struct usb_driver usbd_driver = {
    .name = "usbd",
    .probe = usbd_probe_interface,
    .id_table = usbd_id_table,
};

static int usbd_init(void)
{
    struct sysinfo* sysinfo;
    int i;
    int retval;

    printl("usbd: USB daemon is running\n");

    get_sysinfo(&sysinfo);

    retval = dm_bus_register("usb", &usb_bus_id);
    if (retval) return retval;

#if CONFIG_OF
    boot_params = sysinfo->boot_params;
#endif

    for (i = 0; i < MAX_DRIVERS; i++) {
        struct usbd_driver* drv = &usbd_drivers[i];
        drv->status = DRIVER_UNUSED;
        drv->endpoint = NO_TASK;
        drv->intf = NULL;
    }

    usb_register_driver(&usbd_driver);

    usb_hub_init();

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
        case CLOCK:
            expire_timer(msg->TIMESTAMP);
            break;
        }

        return;
    }

    switch (msg->type) {
    case USB_RQ_INIT:
        do_register_driver(msg);
        msg->RETVAL = SUSPEND;
        break;
    case USB_RQ_SEND_URB:
        do_send_urb(msg);
        msg->RETVAL = SUSPEND;
        break;
    case DM_BUS_ATTR_SHOW:
    case DM_BUS_ATTR_STORE:
        msg->CNT = dm_bus_attr_handle(msg);
        break;
    case DM_DEVICE_ATTR_SHOW:
    case DM_DEVICE_ATTR_STORE:
        dm_device_attr_handle(msg);
        msg->RETVAL = SUSPEND;
        break;
    case DM_DEVICE_BIND:
        msg->RETVAL = do_bind_device(msg);
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

static void usbd_probe_hcd(struct async_work* work)
{
#if CONFIG_USB_PCI
    hcd_pci_scan();
#endif

#if CONFIG_USB_DWC2
    dwc2_scan();
#endif
}

static void usbd_post_init(void)
{
    INIT_ASYNC_WORK(&probe_work, usbd_probe_hcd);
    asyncdrv_enqueue_work(&probe_work);
}

int main()
{
    serv_register_init_fresh_callback(usbd_init);
    serv_init();

    asyncdrv_task(&asyncdriver, MAX_THREADS, usbd_post_init);

    return 0;
}
