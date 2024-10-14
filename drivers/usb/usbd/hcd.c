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

int usb_hcd_add(struct usb_hcd* hcd, int irq)
{
    struct usb_hub* rhdev;
    int retval = 0;

    list_add(&hcd->list, &hcd_list);

    rhdev = usb_create_hub(&hcd->self);
    if (!rhdev) {
        retval = ENOMEM;
        goto rm_list;
    }
    hcd->self.roothub = rhdev;

    if (hcd->driver->setup) {
        retval = hcd->driver->setup(hcd);
        if (retval) return retval;
    }

    if (irq && hcd->driver->irq) {
        hcd->irq = irq;
        hcd->irq_hook = irq;

        retval = irq_setpolicy(irq, 0, &hcd->irq_hook);
        if (retval) return retval;

        retval = irq_enable(&hcd->irq_hook);
        if (retval) return retval;
    } else {
        hcd->irq = 0;
    }

    retval = hcd->driver->start(hcd);
    if (retval) goto rm_irq;

    hcd->self.devnum_next += 1;
    SET_BIT(hcd->self.devmap, 1);

    usb_hcd_poll_rh_status(hcd);

    return 0;

rm_irq:
    if (hcd->irq) {
        irq_rmpolicy(&hcd->irq_hook);
    }

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

int usb_hcd_call_control(struct usb_hcd* hcd, u16 typeReq, u16 wValue,
                         u16 wIndex, char* buf, u16 wLength)
{
    if (!hcd->driver->hub_control) return ENOSYS;

    return hcd->driver->hub_control(hcd, typeReq, wValue, wIndex, buf, wLength);
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
    struct usb_hcd* hcd = bus_to_hcd(urb->dev->bus);
    int retval;

    usb_get_urb(urb);

    retval = map_urb_for_dma(hcd, urb);
    if (!retval) {
        retval = hcd->driver->urb_enqueue(hcd, urb);

        if (retval) unmap_urb_for_dma(hcd, urb);
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
