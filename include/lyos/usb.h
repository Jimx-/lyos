#ifndef _USB_H_
#define _USB_H_

#include <lyos/types.h>

#include <linux/usb/ch9.h>

#define USB_DEVICE_ID_MATCH_VENDOR       0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT      0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO       0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI       0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS    0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS 0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL 0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS    0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS 0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL 0x0200
#define USB_DEVICE_ID_MATCH_INT_NUMBER   0x0400

struct usb_device_id {
    unsigned int match_flags;

    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice_lo;
    u16 bcdDevice_hi;

    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;

    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;

    u8 bInterfaceNumber;
};

#endif
