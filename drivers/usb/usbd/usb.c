#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <stdlib.h>
#include <string.h>

#define __LINUX_ERRNO_EXTENSIONS__
#include <errno.h>

#include "usb.h"
#include "hcd.h"

struct usb_device* usb_alloc_dev(struct usb_device* parent, struct usb_bus* bus,
                                 unsigned int port1)
{
    struct usb_device* dev;
    struct usb_hcd* hcd = bus_to_hcd(bus);

    dev = malloc(sizeof(*dev));
    if (!dev) return NULL;

    if (!usb_get_hcd(hcd)) {
        free(dev);
        return NULL;
    }

    memset(dev, 0, sizeof(*dev));

    kref_init(&dev->kref);
    INIT_LIST_HEAD(&dev->ep0.urb_list);
    dev->ep0.desc.bLength = USB_DT_ENDPOINT_SIZE;
    dev->ep0.desc.bDescriptorType = USB_DT_ENDPOINT;
    usb_enable_endpoint(dev, &dev->ep0, FALSE);

    dev->portnum = port1;
    dev->bus = bus;
    dev->parent = parent;

    return dev;
}

struct usb_device* usb_get_dev(struct usb_device* udev)
{
    if (udev) kref_get(&udev->kref);
    return udev;
}

static void usb_free_dev(struct kref* kref)
{
    struct usb_device* udev = list_entry(kref, struct usb_device, kref);

    free(udev);
}

void usb_put_dev(struct usb_device* udev)
{
    if (udev) {
        kref_put(&udev->kref, usb_free_dev);
    }
}

const char* usb_speed_string(enum usb_device_speed speed)
{
    switch (speed) {
    case USB_SPEED_LOW:
        return "low-speed";
    case USB_SPEED_FULL:
        return "full-speed";
    case USB_SPEED_HIGH:
        return "high-speed";
    case USB_SPEED_WIRELESS:
        return "wireless";
    case USB_SPEED_SUPER:
        return "super-speed";
    case USB_SPEED_SUPER_PLUS:
        return "super-speed-plus";
    default:
        return "UNKNOWN";
    }
}

void usb_init_urb(struct urb* urb)
{
    if (!urb) return;

    memset(urb, 0, sizeof(*urb));
    kref_init(&urb->kref);
    INIT_LIST_HEAD(&urb->urb_list);
}

struct urb* usb_alloc_urb(int iso_packets)
{
    struct urb* urb;
    size_t urb_size =
        sizeof(*urb) + iso_packets * sizeof(struct usb_iso_packet_descriptor);

    urb = malloc(urb_size);
    if (!urb) return NULL;

    usb_init_urb(urb);
    return urb;
}

struct urb* usb_get_urb(struct urb* urb)
{
    if (urb) kref_get(&urb->kref);
    return urb;
}

static void urb_destory(struct kref* kref)
{
    struct urb* urb = list_entry(kref, struct urb, kref);

    if (urb->transfer_flags & URB_FREE_BUFFER) {
        free(urb->transfer_buffer);
    }

    free(urb);
}

void usb_free_urb(struct urb* urb)
{
    if (urb) {
        kref_put(&urb->kref, urb_destory);
    }
}

int usb_submit_urb(struct urb* urb)
{
    struct usb_device* dev;
    struct usb_host_endpoint* ep;
    int xfertype, max;
    int is_out;

    if (!urb || !urb->complete) {
        return EINVAL;
    }

    dev = urb->dev;
    if (!dev) {
        return ENODEV;
    }

    ep = usb_pipe_endpoint(dev, urb->pipe);
    if (!ep) return ENOENT;

    urb->ep = ep;
    urb->status = EINPROGRESS;
    urb->actual_length = 0;

    xfertype = usb_endpoint_type(&ep->desc);
    if (xfertype == USB_ENDPOINT_XFER_CONTROL) {
        struct usb_ctrlrequest* setup =
            (struct usb_ctrlrequest*)urb->setup_packet;

        if (!setup) return ENOEXEC;

        is_out = !(setup->bRequestType & USB_DIR_IN) || !setup->wLength;

        if (setup->wLength != urb->transfer_buffer_length) return EBADR;
    } else {
        is_out = usb_endpoint_dir_out(&ep->desc);
    }

    urb->transfer_flags |= (is_out ? URB_DIR_OUT : URB_DIR_IN);

    max = usb_endpoint_maxp(&ep->desc);
    if (max <= 0) return EMSGSIZE;

    switch (xfertype) {
    case USB_ENDPOINT_XFER_ISOC:
    case USB_ENDPOINT_XFER_INT:
        if (urb->interval <= 0) return EINVAL;

        switch (dev->speed) {
        case USB_SPEED_SUPER_PLUS:
        case USB_SPEED_SUPER:
            if (urb->interval > (1 << 15)) return EINVAL;
            max = 1 << 15;
            break;
        case USB_SPEED_HIGH:
            if (urb->interval > (1024 * 8)) urb->interval = 1024 * 8;
            max = 1024 * 8;
            break;
        case USB_SPEED_FULL:
        case USB_SPEED_LOW:
            if (xfertype == USB_ENDPOINT_XFER_INT) {
                if (urb->interval > 255) return EINVAL;
                max = 128;
            } else {
                if (urb->interval > 1024) urb->interval = 1024;
                max = 1024;
            }
            break;
        default:
            return EINVAL;
        }
        urb->interval = min(max, urb->interval);
    }

    return usb_hcd_submit_urb(urb);
}

struct usb_host_interface*
usb_altnum_to_altsetting(const struct usb_interface* intf, unsigned int altnum)
{
    int i;

    for (i = 0; i < intf->num_altsetting; i++) {
        if (intf->altsetting[i].desc.bAlternateSetting == altnum)
            return &intf->altsetting[i];
    }
    return NULL;
}

static void usb_free_interface(struct kref* kref)
{
    struct usb_interface* intf = list_entry(kref, struct usb_interface, kref);

    free(intf);
}

struct usb_interface* usb_get_interface(struct usb_interface* intf)
{
    if (intf) kref_get(&intf->kref);
    return intf;
}

void usb_put_interface(struct usb_interface* intf)
{
    if (intf) {
        kref_put(&intf->kref, usb_free_interface);
    }
}
