#ifndef _USBD_HCD_H_
#define _USBD_HCD_H_

#include <lyos/types.h>
#include <lyos/kref.h>
#include <lyos/list.h>
#include <linux/usb/ch11.h>

#include "usb.h"

struct hcd_device {
    struct kref kref;
};

struct usb_hcd {
    struct usb_bus self;
    struct kref kref;
    struct list_head list;

    int speed;

    const struct hc_driver* driver;

    void* regs;
    int irq;
    int irq_hook;

    unsigned long hcd_priv[] __attribute__((aligned(sizeof(s64))));
};

static inline struct usb_bus* hcd_to_bus(struct usb_hcd* hcd)
{
    return &hcd->self;
}

static inline struct usb_hcd* bus_to_hcd(struct usb_bus* bus)
{
    return list_entry(bus, struct usb_hcd, self);
}

struct hc_driver {
    const char* description;
    size_t hcd_priv_size;

    int flags;
#define HCD_MEMORY 0x0001
#define HCD_USB11  0x0010
#define HCD_USB2   0x0020
#define HCD_USB3   0x0040
#define HCD_USB31  0x0050
#define HCD_USB32  0x0060
#define HCD_MASK   0x0070

    void (*irq)(struct usb_hcd* hcd);

    int (*setup)(struct usb_hcd* hcd);
    int (*start)(struct usb_hcd* hcd);

    int (*urb_enqueue)(struct usb_hcd* hcd, struct urb* urb);

    int (*map_urb_for_dma)(struct usb_hcd* hcd, struct urb* urb);
    void (*unmap_urb_for_dma)(struct usb_hcd* hcd, struct urb* urb);

    void (*disable_endpoint)(struct usb_hcd* hcd, struct usb_host_endpoint* ep);
    void (*reset_endpoint)(struct usb_hcd* hcd, struct usb_host_endpoint* ep);

    int (*hub_status_data)(struct usb_hcd* hcd, char* buf);
    int (*hub_control)(struct usb_hcd* hcd, u16 typeReq, u16 wValue, u16 wIndex,
                       char* buf, u16 wLength);

    int (*reset_device)(struct usb_hcd* hcd, struct usb_device* udev);
    int (*enable_device)(struct usb_hcd* hcd, struct usb_device* udev);
    int (*address_device)(struct usb_hcd*, struct usb_device* udev);
};

struct usb_hcd* usb_create_hcd(const struct hc_driver* driver);
struct usb_hcd* usb_get_hcd(struct usb_hcd* hcd);
void usb_put_hcd(struct usb_hcd* hcd);
int usb_hcd_add(struct usb_hcd* hcd, int irq);
void usb_hcd_intr(unsigned int mask);
void usb_hcd_poll_rh_status(struct usb_hcd* hcd);

int usb_hcd_submit_urb(struct urb* urb);
int usb_hcd_map_urb_for_dma(struct usb_hcd* hcd, struct urb* urb);
void usb_hcd_unmap_urb_for_dma(struct usb_hcd* hcd, struct urb* urb);

int usb_hcd_link_urb_to_ep(struct usb_hcd* hcd, struct urb* urb);
void usb_hcd_unlink_urb_from_ep(struct usb_hcd* hcd, struct urb* urb);
void usb_hcd_giveback_urb(struct usb_hcd* hcd, struct urb* urb, int status);

#define DeviceRequest ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE) << 8)
#define DeviceOutRequest \
    ((USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE) << 8)

#define InterfaceRequest \
    ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) << 8)

#define EndpointRequest \
    ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT) << 8)
#define EndpointOutRequest \
    ((USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT) << 8)

/* class requests from the USB 2.0 hub spec, table 11-15 */
#define HUB_CLASS_REQ(dir, type, request) ((((dir) | (type)) << 8) | (request))
/* GetBusState and SetHubDescriptor are optional, omitted */
#define ClearHubFeature \
    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_HUB, USB_REQ_CLEAR_FEATURE)
#define ClearPortFeature \
    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, USB_REQ_CLEAR_FEATURE)
#define GetHubDescriptor \
    HUB_CLASS_REQ(USB_DIR_IN, USB_RT_HUB, USB_REQ_GET_DESCRIPTOR)
#define GetHubStatus  HUB_CLASS_REQ(USB_DIR_IN, USB_RT_HUB, USB_REQ_GET_STATUS)
#define GetPortStatus HUB_CLASS_REQ(USB_DIR_IN, USB_RT_PORT, USB_REQ_GET_STATUS)
#define SetHubFeature \
    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_HUB, USB_REQ_SET_FEATURE)
#define SetPortFeature \
    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, USB_REQ_SET_FEATURE)
#define ClearTTBuffer \
    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, HUB_CLEAR_TT_BUFFER)
#define ResetTT    HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, HUB_RESET_TT)
#define GetTTState HUB_CLASS_REQ(USB_DIR_IN, USB_RT_PORT, HUB_GET_TT_STATE)
#define StopTT     HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, HUB_STOP_TT)

void usb_hcd_disable_endpoint(struct usb_device* udev,
                              struct usb_host_endpoint* ep);
void usb_hcd_reset_endpoint(struct usb_device* udev,
                            struct usb_host_endpoint* ep);

#if CONFIG_USB_PCI
void hcd_pci_init(void);
void hcd_pci_scan(void);
int usb_hcd_pci_probe(int devind, const struct hc_driver* driver);
#endif

#endif
