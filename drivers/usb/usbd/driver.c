#include <lyos/const.h>
#include <lyos/usb.h>

#include "usb.h"

int usb_match_device(struct usb_device* dev, const struct usb_device_id* id)
{
    if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
        id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
        id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
        (id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
        (id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
        (id->bDeviceClass != dev->descriptor.bDeviceClass))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
        (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
        (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
        return FALSE;

    return TRUE;
}

int usb_match_one_id_intf(struct usb_device* dev,
                          struct usb_host_interface* intf,
                          const struct usb_device_id* id)
{
    if (dev->descriptor.bDeviceClass == USB_CLASS_VENDOR_SPEC &&
        !(id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
        (id->match_flags &
         (USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS |
          USB_DEVICE_ID_MATCH_INT_PROTOCOL | USB_DEVICE_ID_MATCH_INT_NUMBER)))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
        (id->bInterfaceClass != intf->desc.bInterfaceClass))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
        (id->bInterfaceSubClass != intf->desc.bInterfaceSubClass))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
        (id->bInterfaceProtocol != intf->desc.bInterfaceProtocol))
        return FALSE;

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_NUMBER) &&
        (id->bInterfaceNumber != intf->desc.bInterfaceNumber))
        return FALSE;

    return FALSE;
}
