#ifndef _USBD_HUB_H_
#define _USBD_HUB_H_

#include <linux/usb/ch11.h>

#include "usb.h"

struct usb_hub {
    struct usb_device* hdev;
    struct urb* urb;

    u8 buffer[8];

    union {
        struct usb_hub_status hub;
        struct usb_port_status port;
    } status;

    unsigned long event_bits[1];

    int maxchild;
    struct usb_device* ports[USB_MAXCHILDREN];
};

struct usb_hub* usb_create_hub(struct usb_device* hdev);
void usb_hub_handle_status_data(struct usb_hub* hub, const char* buffer,
                                int length);

int usb_clear_port_feature(struct usb_device* hdev, int port1, int feature);

int usb_new_device(struct usb_device* udev);

#endif
