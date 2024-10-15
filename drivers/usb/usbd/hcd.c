#include <lyos/types.h>
#include <lyos/const.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include <lyos/irqctl.h>
#include <errno.h>
#include <lyos/usb.h>

#include "usb.h"
#include "hcd.h"
#include "hub.h"

static const u8 usb11_rh_dev_descriptor[18] = {
    0x12,                /*  __u8  bLength; */
    USB_DT_DEVICE,       /* __u8 bDescriptorType; Device */
    0x10,          0x01, /*  __le16 bcdUSB; v1.1 */

    0x09, /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00, /*  __u8  bDeviceSubClass; */
    0x00, /*  __u8  bDeviceProtocol; [ low/full speeds only ] */
    0x40, /*  __u8  bMaxPacketSize0; 64 Bytes */

    0x6b,          0x1d, /*  __le16 idVendor; Linux Foundation 0x1d6b */
    0x01,          0x00, /*  __le16 idProduct; device 0x0001 */
    0x01,          0x00, /*  __le16 bcdDevice */

    0x03, /*  __u8  iManufacturer; */
    0x02, /*  __u8  iProduct; */
    0x01, /*  __u8  iSerialNumber; */
    0x01  /*  __u8  bNumConfigurations; */
};

static DEF_LIST(hcd_list);

static void usb_bus_init(struct usb_bus* bus)
{
    memset(&bus->devmap, 0, sizeof(bus->devmap));
    bus->devnum_next = 1;

    bus->roothub = NULL;
    bus->busnum = -1;
}

struct usb_hcd* usb_create_hcd(const struct hc_driver* driver)
{
    struct usb_hcd* hcd;
    size_t hcd_size;

    hcd_size = sizeof(*hcd) + driver->hcd_priv_size;
    hcd = malloc(hcd_size);
    if (!hcd) {
        return hcd;
    }

    memset(hcd, 0, hcd_size);

    kref_init(&hcd->kref);

    usb_bus_init(&hcd->self);

    hcd->driver = driver;
    hcd->speed = driver->flags & HCD_MASK;

    return hcd;
}

struct usb_hcd* usb_get_hcd(struct usb_hcd* hcd)
{
    if (hcd) kref_get(&hcd->kref);
    return hcd;
}

static void hcd_release(struct kref* kref)
{
    struct usb_hcd* hcd = list_entry(kref, struct usb_hcd, kref);
    free(hcd);
}

void usb_put_hcd(struct usb_hcd* hcd)
{
    if (hcd) kref_put(&hcd->kref, hcd_release);
}

static int register_roothub(struct usb_hcd* hcd)
{
    struct usb_device* hdev = hcd->self.roothub->hdev;
    struct usb_device_descriptor* descr;
    const int devnum = 1;
    int retval = 0;

    hdev->devnum = devnum;
    hcd->self.devnum_next = devnum + 1;
    SET_BIT(hcd->self.devmap, devnum);

    hdev->ep0.desc.wMaxPacketSize = cpu_to_le16(64);
    descr = usb_get_device_descriptor(hdev);
    if (!descr) return EIO;

    hdev->descriptor = *descr;
    free(descr);

    return retval;
}

int usb_hcd_add(struct usb_hcd* hcd, int irq)
{
    struct usb_device* rhdev;
    struct usb_hub* roothub;
    int retval = 0;

    list_add(&hcd->list, &hcd_list);

    rhdev = usb_alloc_dev(NULL, &hcd->self, 0);
    if (!rhdev) {
        retval = ENOMEM;
        goto rm_list;
    }

    roothub = usb_create_hub(rhdev);
    if (!roothub) {
        retval = ENOMEM;
        goto free_hdev;
    }
    hcd->self.roothub = roothub;

    if (hcd->driver->setup) {
        retval = hcd->driver->setup(hcd);
        if (retval) goto free_hub;
    }

    if (irq && hcd->driver->irq) {
        hcd->irq = irq;
        hcd->irq_hook = irq;

        retval = irq_setpolicy(irq, 0, &hcd->irq_hook);
        if (retval) goto free_hub;

        retval = irq_enable(&hcd->irq_hook);
        if (retval) goto free_hub;
    } else {
        hcd->irq = 0;
    }

    retval = hcd->driver->start(hcd);
    if (retval) goto rm_irq;

    retval = register_roothub(hcd);
    if (retval) goto rm_irq;

    usb_hcd_poll_rh_status(hcd);

    return 0;

rm_irq:
    if (hcd->irq) {
        irq_rmpolicy(&hcd->irq_hook);
    }

free_hub:
free_hdev:
    usb_put_dev(rhdev);

rm_list:
    list_del(&hcd->list);

    return retval;
}

void usb_hcd_intr(unsigned int mask)
{
    struct usb_hcd* hcd;

    list_for_each_entry(hcd, &hcd_list, list)
    {
        if (hcd->driver->irq && (mask & (1UL << hcd->irq)))
            hcd->driver->irq(hcd);
    }
}

void usb_hcd_poll_rh_status(struct usb_hcd* hcd)
{
    char buffer[6];
    int length;

    if (!hcd->driver->hub_status_data) return;

    length = hcd->driver->hub_status_data(hcd, buffer);
    if (!length) return;

    usb_hub_handle_status_data(hcd->self.roothub, buffer, length);
}

static int rh_call_control(struct usb_hcd* hcd, struct urb* urb)
{
    struct usb_ctrlrequest* cmd;
    u16 typeReq, wValue, wIndex, wLength;
    u8* ubuf = urb->transfer_buffer;
    unsigned int len = 0;
    u16 tbuf_size;
    u8* tbuf = NULL;
    const u8* bufp;
    int retval;

    retval = usb_hcd_link_urb_to_ep(hcd, urb);
    if (retval) return retval;

    urb->hc_priv = hcd;

    cmd = (struct usb_ctrlrequest*)urb->setup_packet;
    typeReq = (cmd->bRequestType << 8) | cmd->bRequest;
    wValue = le16_to_cpu(cmd->wValue);
    wIndex = le16_to_cpu(cmd->wIndex);
    wLength = le16_to_cpu(cmd->wLength);

    tbuf_size = max(sizeof(struct usb_hub_descriptor), wLength);
    tbuf = malloc(tbuf_size);
    if (!tbuf) {
        retval = ENOMEM;
        goto err_alloc;
    }

    bufp = tbuf;

    urb->actual_length = 0;

    switch (typeReq) {
    case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
        switch (wValue & 0xff00) {
        case USB_DT_DEVICE << 8:
            switch (hcd->speed) {
            case HCD_USB11:
                bufp = usb11_rh_dev_descriptor;
                break;
            default:
                goto error;
            }

            len = 18;
            break;
        }
        break;

    default:
        switch (typeReq) {
        case GetHubStatus:
            len = 4;
            break;
        case GetPortStatus:
            if (wValue == HUB_PORT_STATUS)
                len = 4;
            else
                len = 8;
            break;
        case GetHubDescriptor:
            len = sizeof(struct usb_hub_descriptor);
            break;
        }

        retval = hcd->driver->hub_control(hcd, typeReq, wValue, wIndex,
                                          (char*)tbuf, wLength);
        if (retval < 0)
            retval = -retval;
        else if (retval > 0) {
            len = retval;
            retval = 0;
        }

        break;

    error:
        retval = EPIPE;
    }

    if (retval) len = 0;

    if (len) {
        if (urb->transfer_buffer_length < len)
            len = urb->transfer_buffer_length;

        urb->actual_length = len;

        memcpy(ubuf, bufp, len);
    }

    free(tbuf);

err_alloc:
    usb_hcd_unlink_urb_from_ep(hcd, urb);
    usb_hcd_giveback_urb(hcd, urb, retval);

    return retval;
}

void usb_hcd_disable_endpoint(struct usb_device* udev,
                              struct usb_host_endpoint* ep)
{
    struct usb_hcd* hcd;

    hcd = bus_to_hcd(udev->bus);
    if (hcd->driver->disable_endpoint) hcd->driver->disable_endpoint(hcd, ep);
}

void usb_hcd_reset_endpoint(struct usb_device* udev,
                            struct usb_host_endpoint* ep)
{
    struct usb_hcd* hcd = bus_to_hcd(udev->bus);

    if (hcd->driver->reset_endpoint)
        hcd->driver->reset_endpoint(hcd, ep);
    else {
        int epnum = usb_endpoint_num(&ep->desc);
        int is_out = usb_endpoint_dir_out(&ep->desc);
        int is_control = usb_endpoint_xfer_control(&ep->desc);

        usb_settoggle(udev, epnum, is_out, 0);
        if (is_control) usb_settoggle(udev, epnum, !is_out, 0);
    }
}

static int rh_urb_enqueue(struct usb_hcd* hcd, struct urb* urb)
{
    if (usb_endpoint_xfer_control(&urb->ep->desc))
        return rh_call_control(hcd, urb);
    return EINVAL;
}

static int map_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    if (hcd->driver->map_urb_for_dma)
        return hcd->driver->map_urb_for_dma(hcd, urb);

    return usb_hcd_map_urb_for_dma(hcd, urb);
}

int usb_hcd_map_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    int retval = 0;

    if (usb_endpoint_xfer_control(&urb->ep->desc)) {
        retval = umap(SELF, UMT_VADDR, (vir_bytes)urb->setup_packet,
                      sizeof(struct usb_ctrlrequest), &urb->setup_phys);

        if (retval) return retval;
    }

    if (urb->transfer_buffer_length != 0) {
        retval = umap(SELF, UMT_VADDR, (vir_bytes)urb->transfer_buffer,
                      urb->transfer_buffer_length, &urb->transfer_phys);

        if (retval) return retval;
    }

    return retval;
}

static void unmap_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    if (hcd->driver->unmap_urb_for_dma)
        hcd->driver->unmap_urb_for_dma(hcd, urb);
    else
        usb_hcd_unmap_urb_for_dma(hcd, urb);
}

void usb_hcd_unmap_urb_for_dma(struct usb_hcd* hcd, struct urb* urb) {}

int usb_hcd_submit_urb(struct urb* urb)
{
    struct usb_device* udev = urb->dev;
    struct usb_hcd* hcd = bus_to_hcd(udev->bus);
    int retval;

    usb_get_urb(urb);

    if (udev->parent == NULL) {
        retval = rh_urb_enqueue(hcd, urb);
    } else {
        retval = map_urb_for_dma(hcd, urb);
        if (!retval) {
            retval = hcd->driver->urb_enqueue(hcd, urb);

            if (retval) unmap_urb_for_dma(hcd, urb);
        }
    }

    if (retval) {
        urb->hc_priv = NULL;
        INIT_LIST_HEAD(&urb->urb_list);
        usb_put_urb(urb);
    }

    return retval;
}

int usb_hcd_link_urb_to_ep(struct usb_hcd* hcd, struct urb* urb)
{
    urb->unlinked = 0;
    list_add_tail(&urb->urb_list, &urb->ep->urb_list);

    return 0;
}

void usb_hcd_unlink_urb_from_ep(struct usb_hcd* hcd, struct urb* urb)
{
    list_del(&urb->urb_list);
    INIT_LIST_HEAD(&urb->urb_list);
}

void usb_hcd_giveback_urb(struct usb_hcd* hcd, struct urb* urb, int status)
{
    if (!urb->unlinked) urb->unlinked = status;

    if ((urb->transfer_flags & URB_SHORT_NOT_OK) &&
        urb->actual_length < urb->transfer_buffer_length && !status)
        status = EIO;

    unmap_urb_for_dma(hcd, urb);

    urb->status = status;

    urb->complete(urb);

    usb_free_urb(urb);
}
