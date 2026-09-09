#include <lyos/types.h>
#include <lyos/compile.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <lyos/vm.h>
#include <lyos/sysutils.h>
#include <lyos/irqctl.h>
#include <errno.h>
#include <lyos/usb.h>
#include <lyos/idr.h>
#include <asm/page.h>

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

static const u8 usb2_rh_dev_descriptor[18] = {
    0x12,                /* bLength */
    USB_DT_DEVICE,       /* bDescriptorType */
    0x00,          0x02, /* bcdUSB 2.0 */
    0x09,                /* hub class */
    0x00,                /* subclass */
    0x00,                /* protocol */
    0x40,                /* bMaxPacketSize0 */
    0x6b,          0x1d, /* Linux Foundation */
    0x02,          0x00, /* USB 2.0 root hub */
    0x01,          0x00, /* bcdDevice */
    0x03,                /* iManufacturer */
    0x02,                /* iProduct */
    0x01,                /* iSerialNumber */
    0x01                 /* bNumConfigurations */
};

static const u8 usb3_rh_dev_descriptor[18] = {
    0x12,                /*  __u8  bLength; */
    USB_DT_DEVICE,       /* __u8 bDescriptorType; Device */
    0x00,          0x03, /*  __le16 bcdUSB; v3.0 */

    0x09, /*  __u8  bDeviceClass; HUB_CLASSCODE */
    0x00, /*  __u8  bDeviceSubClass; */
    0x03, /*  __u8  bDeviceProtocol; SuperSpeed */
    0x09, /*  __u8  bMaxPacketSize0; 2^9 = 512 Bytes */

    0x6b,          0x1d, /*  __le16 idVendor; Linux Foundation 0x1d6b */
    0x03,          0x00, /*  __le16 idProduct; device 0x0003 */
    0x01,          0x00, /*  __le16 bcdDevice */

    0x03, /*  __u8  iManufacturer; */
    0x02, /*  __u8  iProduct; */
    0x01, /*  __u8  iSerialNumber; */
    0x01  /*  __u8  bNumConfigurations; */
};

static const u8 fs_rh_config_descriptor[] = {
    /* one configuration */
    0x09,          /*  __u8  bLength; */
    USB_DT_CONFIG, /* __u8 bDescriptorType; Configuration */
    0x19, 0x00,    /*  __le16 wTotalLength; */
    0x01,          /*  __u8  bNumInterfaces; (1) */
    0x01,          /*  __u8  bConfigurationValue; */
    0x00,          /*  __u8  iConfiguration; */
    0xc0,          /*  __u8  bmAttributes; */
    0x00,          /*  __u8  MaxPower; */

    /* one interface */
    0x09,             /*  __u8  if_bLength; */
    USB_DT_INTERFACE, /* __u8 if_bDescriptorType; Interface */
    0x00,             /*  __u8  if_bInterfaceNumber; */
    0x00,             /*  __u8  if_bAlternateSetting; */
    0x01,             /*  __u8  if_bNumEndpoints; */
    0x09,             /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,             /*  __u8  if_bInterfaceSubClass; */
    0x00,             /*  __u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
    0x00,             /*  __u8  if_iInterface; */

    /* one endpoint (status change endpoint) */
    0x07,            /*  __u8  ep_bLength; */
    USB_DT_ENDPOINT, /* __u8 ep_bDescriptorType; Endpoint */
    0x81,            /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,            /*  __u8  ep_bmAttributes; Interrupt */
    0x02, 0x00,      /*  __le16 ep_wMaxPacketSize; 1 + (MAX_ROOT_PORTS / 8) */
    0xff             /*  __u8  ep_bInterval; (255ms -- usb 2.0 spec) */
};

static const u8 ss_rh_config_descriptor[] = {
    /* one configuration */
    0x09,          /*  __u8  bLength; */
    USB_DT_CONFIG, /* __u8 bDescriptorType; Configuration */
    0x19, 0x00,    /*  __le16 wTotalLength; */
    0x01,          /*  __u8  bNumInterfaces; (1) */
    0x01,          /*  __u8  bConfigurationValue; */
    0x00,          /*  __u8  iConfiguration; */
    0xc0,          /*  __u8  bmAttributes; */
    0x00,          /*  __u8  MaxPower; */

    /* one interface */
    0x09,             /*  __u8  if_bLength; */
    USB_DT_INTERFACE, /* __u8 if_bDescriptorType; Interface */
    0x00,             /*  __u8  if_bInterfaceNumber; */
    0x00,             /*  __u8  if_bAlternateSetting; */
    0x01,             /*  __u8  if_bNumEndpoints; */
    0x09,             /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
    0x00,             /*  __u8  if_bInterfaceSubClass; */
    0x00,             /*  __u8  if_bInterfaceProtocol; SuperSpeed */
    0x00,             /*  __u8  if_iInterface; */

    /* one endpoint (status change endpoint) */
    0x07,            /*  __u8  ep_bLength; */
    USB_DT_ENDPOINT, /* __u8 ep_bDescriptorType; Endpoint */
    0x81,            /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,            /*  __u8  ep_bmAttributes; Interrupt */
    0x02, 0x00,      /*  __le16 ep_wMaxPacketSize; 1 + (MAX_ROOT_PORTS / 8) */
    0xff             /*  __u8  ep_bInterval; (255ms -- usb 2.0 spec) */
};

static DEF_LIST(hcd_list);

static struct idr usb_bus_idr;

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
    hcd->product_desc =
        driver->product_desc ? driver->product_desc : "USB Host Controller";

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

static int usb_register_bus(struct usb_bus* bus)
{
    static int idr_inited = FALSE;
    int busnum;

    if (!idr_inited) {
        idr_init(&usb_bus_idr);
        idr_inited = TRUE;
    }

    busnum = idr_alloc(&usb_bus_idr, bus, 1, 0);
    if (busnum < 0) return E2BIG;

    bus->busnum = busnum;
    return 0;
}

static int register_roothub(struct usb_hcd* hcd)
{
    struct usb_device* hdev = hcd->self.roothub;
    struct usb_device_descriptor* descr;
    const int devnum = 1;
    int retval;

    hdev->devnum = devnum;
    hcd->self.devnum_next = devnum + 1;
    SET_BIT(hcd->self.devmap, devnum);

    hdev->ep0.desc.wMaxPacketSize =
        cpu_to_le16((hcd->speed == HCD_USB3) ? 512 : 64);
    descr = usb_get_device_descriptor(hdev);
    if (!descr) return EIO;

    hdev->descriptor = *descr;
    free(descr);

    retval = usb_new_device(hdev);

    return retval;
}

int usb_hcd_add(struct usb_hcd* hcd, int irq)
{
    struct usb_device* rhdev;
    int retval = 0;

    list_add(&hcd->list, &hcd_list);

    retval = usb_register_bus(&hcd->self);
    if (retval) return retval;

    rhdev = usb_alloc_dev(NULL, &hcd->self, 0);
    if (!rhdev) {
        retval = ENOMEM;
        goto rm_list;
    }
    hcd->self.roothub = rhdev;

    switch (hcd->speed) {
    case HCD_USB11:
        rhdev->speed = USB_SPEED_FULL;
        break;
    case HCD_USB2:
        rhdev->speed = USB_SPEED_HIGH;
        break;
    case HCD_USB3:
        rhdev->speed = USB_SPEED_SUPER;
        break;
    default:
        retval = EINVAL;
        goto free_hdev;
    }

    if (hcd->driver->setup) {
        retval = hcd->driver->setup(hcd);
        if (retval) goto free_hdev;
    }

    if (irq && hcd->driver->irq) {
        hcd->irq = irq;
        hcd->irq_hook = irq;

        retval = irq_setpolicy(irq, 0, &hcd->irq_hook);
        if (retval) goto free_hdev;

        retval = irq_enable(&hcd->irq_hook);
        if (retval) goto free_hdev;
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
    struct urb* urb;
    char buffer[6];
    int length;
    int status;

    if (!hcd->driver->hub_status_data) return;

    length = hcd->driver->hub_status_data(hcd, buffer);
    if (length > 0) {
        urb = hcd->status_urb;
        if (urb) {
            hcd->status_urb = NULL;

            if (urb->transfer_buffer_length >= length) {
                status = 0;
            } else {
                status = EOVERFLOW;
                length = urb->transfer_buffer_length;
            }

            urb->actual_length = length;
            memcpy(urb->transfer_buffer, buffer, length);

            usb_hcd_unlink_urb_from_ep(hcd, urb);
            usb_hcd_giveback_urb(hcd, urb, status);
        }
    }
}

static unsigned ascii2desc(char const* s, u8* buf, unsigned len)
{
    unsigned n, t = 2 + 2 * strlen(s);

    if (t > 254) t = 254;
    if (len > t) len = t;

    t += USB_DT_STRING << 8;

    n = len;
    while (n--) {
        *buf++ = t;
        if (!n--) break;
        *buf++ = t >> 8;
        t = (unsigned char)*s++;
    }
    return len;
}

static unsigned rh_string(int id, struct usb_hcd const* hcd, u8* data,
                          unsigned len)
{
    char buf[100];
    char const* s;
    static char const langids[4] = {4, USB_DT_STRING, 0x09, 0x04};

    switch (id) {
    case 0:
        if (len > 4) len = 4;
        memcpy(data, langids, len);
        return len;
    case 1:
        snprintf(buf, sizeof(buf), "usb%d", hcd->self.busnum);
        s = buf;
        break;
    case 2:
        s = hcd->product_desc;
        break;
    case 3:
        snprintf(buf, sizeof(buf), "Lyos %s %s", UTS_RELEASE,
                 hcd->driver->description);
        s = buf;
        break;
    default:
        return 0;
    }

    return ascii2desc(s, data, len);
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
    case DeviceRequest | USB_REQ_GET_CONFIGURATION:
        tbuf[0] = 1;
        len = 1;
    case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
        break;
    case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
        switch (wValue & 0xff00) {
        case USB_DT_DEVICE << 8:
            switch (hcd->speed) {
            case HCD_USB11:
                bufp = usb11_rh_dev_descriptor;
                break;
            case HCD_USB2:
                bufp = usb2_rh_dev_descriptor;
                break;
            case HCD_USB3:
                bufp = usb3_rh_dev_descriptor;
                break;
            default:
                goto error;
            }

            len = 18;
            break;

        case USB_DT_CONFIG << 8:
            switch (hcd->speed) {
            case HCD_USB11:
            case HCD_USB2:
                bufp = fs_rh_config_descriptor;
                len = sizeof(fs_rh_config_descriptor);
                break;
            case HCD_USB3:
                bufp = ss_rh_config_descriptor;
                len = sizeof(ss_rh_config_descriptor);
                break;
            default:
                goto error;
            }

            break;

        case USB_DT_STRING << 8:
            if ((wValue & 0xff) < 4)
                urb->actual_length =
                    rh_string(wValue & 0xff, hcd, ubuf, wLength);
            else
                goto error;

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
        hcd->driver->reset_endpoint(hcd, udev, ep);
    else {
        int epnum = usb_endpoint_num(&ep->desc);
        int is_out = usb_endpoint_dir_out(&ep->desc);
        int is_control = usb_endpoint_xfer_control(&ep->desc);

        usb_settoggle(udev, epnum, is_out, 0);
        if (is_control) usb_settoggle(udev, epnum, !is_out, 0);
    }
}

static int rh_queue_status(struct usb_hcd* hcd, struct urb* urb)
{
    int retval;

    if (hcd->status_urb) return EINVAL;

    retval = usb_hcd_link_urb_to_ep(hcd, urb);
    if (retval) return retval;

    hcd->status_urb = urb;
    urb->hc_priv = hcd;

    return 0;
}

static int rh_urb_enqueue(struct usb_hcd* hcd, struct urb* urb)
{
    if (usb_endpoint_xfer_int(&urb->ep->desc)) return rh_queue_status(hcd, urb);
    if (usb_endpoint_xfer_control(&urb->ep->desc))
        return rh_call_control(hcd, urb);
    return EINVAL;
}

/* The kernel stores the va2pa failure sentinel in the output even when the
 * syscall reports success; userspace must reject it itself. */
#define UMAP_BAD_PA ((phys_bytes)-1)

/* Touch every page of [buf, buf + len) so that lazily-mapped anonymous
 * pages exist before address translation; va2pa fails on untouched pages. */
static void touch_buffer_pages(void* buf, size_t len)
{
    unsigned long addr = (unsigned long)buf & ~(unsigned long)(ARCH_PG_SIZE - 1);
    unsigned long end = (unsigned long)buf + len;
    volatile char sink = 0;

    for (; addr < end; addr += ARCH_PG_SIZE) sink = *(volatile char*)addr;
    (void)sink;
}

/* Discover the physical runs of [buf, buf + len) and fill the caller's
 * table (allocated with capacity for every page spanned).  Merges page
 * steps whose physical addresses continue the previous run.  Returns the
 * number of populated entries, or a negative Lyos error code. */
static int build_sg_for_buffer(struct scatterlist* sgl, void* buf,
                               unsigned int len)
{
    vir_bytes cur = (vir_bytes)buf;
    unsigned int remaining = len;
    int nents = 0;

    while (remaining > 0) {
        vir_bytes page_span = ARCH_PG_SIZE - (cur & (ARCH_PG_SIZE - 1));
        unsigned int run = page_span < remaining ? page_span : remaining;
        phys_bytes pa;
        int retval;

        /* size is unused by the kernel for UMT_VADDR */
        retval = umap(SELF, UMT_VADDR, cur, run, &pa);
        if (retval) return -retval;
        if (pa == UMAP_BAD_PA) return -EFAULT;

        if (nents > 0 && pa == sg_dma_address(&sgl[nents - 1]) +
                                        sg_dma_len(&sgl[nents - 1])) {
            sgl[nents - 1].length += run;
        } else {
            sg_set_buf(&sgl[nents], (void*)cur, run);
            sg_dma_address(&sgl[nents]) = pa;
            nents++;
        }

        cur += run;
        remaining -= run;
    }

    return nents;
}

static int map_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    if (hcd->driver->map_urb_for_dma)
        return hcd->driver->map_urb_for_dma(hcd, urb);

    return usb_hcd_map_urb_for_dma(hcd, urb);
}

int usb_hcd_map_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    unsigned long buf = (unsigned long)urb->transfer_buffer;
    unsigned int len = urb->transfer_buffer_length;
    unsigned int pages;
    struct scatterlist* sgl;
    int is_control = usb_endpoint_xfer_control(&urb->ep->desc);
    int nents;
    int retval;

    if (is_control) {
        phys_bytes setup_dma;
        size_t setup_len = sizeof(struct usb_ctrlrequest);

        touch_buffer_pages(urb->setup_packet, setup_len);

        retval = umap(SELF, UMT_VADDR, (vir_bytes)urb->setup_packet,
                      setup_len, &setup_dma);
        if (retval) return retval;
        if (setup_dma == UMAP_BAD_PA) return EFAULT;

        /* The SETUP packet must not cross a page; within one page the
         * physical translation is contiguous. */
        if (((unsigned long)urb->setup_packet & ~(unsigned long)(ARCH_PG_SIZE - 1)) !=
            (((unsigned long)urb->setup_packet + setup_len - 1) &
             ~(unsigned long)(ARCH_PG_SIZE - 1)))
            return EFAULT;

        urb->setup_dma = setup_dma;
    }

    if (len == 0) return 0;

    if (len > (unsigned long)-1 - buf) return EOVERFLOW;

    touch_buffer_pages(urb->transfer_buffer, len);

    pages = ((buf & (ARCH_PG_SIZE - 1)) + len + ARCH_PG_SIZE - 1) / ARCH_PG_SIZE;
    if (pages > ((size_t)-1) / sizeof(struct scatterlist)) return ENOMEM;

    sgl = malloc(pages * sizeof(struct scatterlist));
    if (!sgl) return ENOMEM;

    sg_init_table(sgl, pages);

    nents = build_sg_for_buffer(sgl, urb->transfer_buffer, len);
    if (nents < 0) {
        free(sgl);
        return -nents;
    }

    if (nents > 1 || (hcd->driver->dma_alignment &&
                      (sg_dma_address(sgl) & (hcd->driver->dma_alignment - 1)))) {
        /* A discovered physical run can end midway through a USB packet.
         * Lyos transfer buffers are plain byte buffers, so their runs are
         * not guaranteed to be multiples of the endpoint maximum packet
         * size (unlike Linux, where the block layer supplies aligned SG
         * entries).  Splitting a transfer at an unaligned run boundary
         * would emit a short packet mid-stream and corrupt the transfer
         * on the wire.  Bounce fragmented payloads through one
         * physically contiguous mapping instead; HCDs then always see a
         * single run, and mid-transfer splits they perform themselves are
         * maxp-aligned (e.g. OHCI's 4096-byte TD chunks).  A controller
         * alignment requirement can also force bouncing a single run. */
        void* bounce;
        size_t map_len = (size_t)pages * ARCH_PG_SIZE;

        bounce = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                      MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE,
                      -1, 0);
        if (bounce == MAP_FAILED) {
            free(sgl);
            return ENOMEM;
        }

        if (usb_pipeout(urb->pipe))
            memcpy(bounce, urb->transfer_buffer, len);

        sg_init_table(sgl, pages);
        nents = build_sg_for_buffer(sgl, bounce, len);
        if (nents != 1) {
            /* MAP_CONTIG must give one physical run */
            munmap(bounce, map_len);
            free(sgl);
            return EFAULT;
        }

        urb->bounce_buffer = bounce;
    }

    /* Capacity is separate from the populated count: move the end marker
     * from the final allocated slot to the final populated entry. */
    if (nents < pages) {
        sg_unmark_end(&sgl[pages - 1]);
        sg_mark_end(&sgl[nents - 1]);
    }

    urb->sg = sgl;
    urb->num_sgs = urb->num_mapped_sgs = nents;
    urb->transfer_dma = sg_dma_address(&sgl[0]);

    return 0;
}

/* Called by 32-bit HCDs before publishing any descriptors.  The mmap API
 * has no DMA mask argument, so verify the replacement as well as the
 * original allocation; never truncate an unaddressable SETUP pointer. */
int usb_hcd_setup_dma32(struct urb* urb, unsigned int alignment)
{
    void* bounce;
    phys_bytes dma;
    int retval;
    size_t len = sizeof(struct usb_ctrlrequest);

    if (!usb_pipecontrol(urb->pipe)) return 0;
    if (urb->setup_dma <= (phys_bytes)0xffffffffUL - (len - 1) &&
        !(urb->setup_dma & (alignment - 1)))
        return 0;

    bounce = mmap(NULL, ARCH_PG_SIZE, PROT_READ | PROT_WRITE,
                  MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE,
                  -1, 0);
    if (bounce == MAP_FAILED) return ENOMEM;
    memcpy(bounce, urb->setup_packet, len);
    retval = umap(SELF, UMT_VADDR, (vir_bytes)bounce, len, &dma);
    if (!retval && dma == UMAP_BAD_PA) retval = EFAULT;
    if (!retval && dma > (phys_bytes)0xffffffffUL - (len - 1)) retval = ERANGE;
    if (retval) {
        munmap(bounce, ARCH_PG_SIZE);
        return retval;
    }

    urb->setup_bounce_buffer = bounce;
    urb->setup_dma = dma;
    return 0;
}

static void unmap_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    if (hcd->driver->unmap_urb_for_dma)
        hcd->driver->unmap_urb_for_dma(hcd, urb);
    else
        usb_hcd_unmap_urb_for_dma(hcd, urb);
}

void usb_hcd_unmap_urb_for_dma(struct usb_hcd* hcd, struct urb* urb)
{
    if (urb->setup_bounce_buffer) {
        munmap(urb->setup_bounce_buffer, ARCH_PG_SIZE);
        urb->setup_bounce_buffer = NULL;
    }

    if (urb->bounce_buffer) {
        unsigned long buf = (unsigned long)urb->transfer_buffer;
        unsigned int len = urb->transfer_buffer_length;
        size_t map_len =
            (size_t)(((buf & (ARCH_PG_SIZE - 1)) + len + ARCH_PG_SIZE - 1) /
                     ARCH_PG_SIZE) *
            ARCH_PG_SIZE;

        if (usb_pipein(urb->pipe) && urb->actual_length) {
            size_t n = urb->actual_length;

            if (n > urb->transfer_buffer_length)
                n = urb->transfer_buffer_length;
            memcpy(urb->transfer_buffer, urb->bounce_buffer, n);
        }

        munmap(urb->bounce_buffer, map_len);
        urb->bounce_buffer = NULL;
    }

    if (urb->sg) {
        free(urb->sg);
        urb->sg = NULL;
    }

    urb->num_sgs = urb->num_mapped_sgs = 0;
    urb->transfer_dma = 0;
    urb->setup_dma = 0;
}

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
