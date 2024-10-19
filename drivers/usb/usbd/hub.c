#include <lyos/const.h>
#include <stdlib.h>
#include <lyos/sysutils.h>
#include <lyos/usb.h>
#include <errno.h>
#include <unistd.h>

#include "hcd.h"
#include "hub.h"

static const char* name = "usb-hub";

#define PORT_RESET_TRIES     5
#define SET_ADDRESS_TRIES    2
#define GET_DESCRIPTOR_TRIES 2
#define GET_MAXPACKET0_TRIES 3
#define PORT_INIT_TRIES      4

#define HUB_ROOT_RESET_TIME  60
#define HUB_SHORT_RESET_TIME 10
#define HUB_BH_RESET_TIME    50
#define HUB_LONG_RESET_TIME  200
#define HUB_RESET_TIMEOUT    800

static int hub_configure(struct usb_hub* hub);
static void hub_irq(struct urb* urb);

struct usb_hub* usb_create_hub(struct usb_device* hdev)
{
    struct usb_hub* hub;
    int retval;

    hub = malloc(sizeof(*hub));
    if (!hub) return NULL;

    hub->hdev = hdev;
    usb_get_dev(hdev);

    hub->maxchild = USB_MAXCHILDREN;

    return hub;

put_dev:
    usb_put_dev(hdev);

    free(hub);
    return NULL;
}

static void assign_devnum(struct usb_device* udev)
{
    int devnum;
    struct usb_bus* bus = udev->bus;
    int i;

    for (i = bus->devnum_next; i < 128; i++) {
        if (!GET_BIT(bus->devmap, i)) break;
    }
    if (i >= 128) {
        for (i = 1; i < bus->devnum_next; i++) {
            if (!GET_BIT(bus->devmap, i)) break;
        }
    }
    devnum = i;
    bus->devnum_next = (devnum >= 127 ? 1 : devnum + 1);

    if (devnum < 128) {
        SET_BIT(bus->devmap, devnum);
        udev->devnum = devnum;
    }
}

static void release_devnum(struct usb_device* udev)
{
    if (udev->devnum > 0) {
        UNSET_BIT(udev->bus->devmap, udev->devnum);
        udev->devnum = -1;
    }
}
static void update_devnum(struct usb_device* udev, int devnum)
{
    udev->devnum = devnum;
    if (!udev->devaddr) udev->devaddr = (u8)devnum;
}

static int hub_enable_device(struct usb_device* udev)
{
    struct usb_hcd* hcd = bus_to_hcd(udev->bus);

    if (!hcd->driver->enable_device) return 0;

    return hcd->driver->enable_device(hcd, udev);
}

int usb_clear_port_feature(struct usb_device* hdev, int port1, int feature)
{
    return usb_control_msg(hdev, usb_sndctrlpipe(hdev, 0),
                           USB_REQ_CLEAR_FEATURE, USB_RT_PORT, feature, port1,
                           NULL, 0);
}

static int set_port_feature(struct usb_device* hdev, int port1, int feature)
{
    return usb_control_msg(hdev, usb_sndctrlpipe(hdev, 0), USB_REQ_SET_FEATURE,
                           USB_RT_PORT, feature, port1, NULL, 0);
}

#define USB_STS_RETRIES 5

static int get_port_status(struct usb_device* hdev, int port1, void* data,
                           u16 value, u16 length)
{
    int i, status = -ETIMEDOUT;

    for (i = 0;
         i < USB_STS_RETRIES && (status == -ETIMEDOUT || status == -EPIPE);
         i++) {
        status = usb_control_msg(hdev, usb_rcvctrlpipe(hdev, 0),
                                 USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_PORT,
                                 value, port1, data, length);
    }
    return status;
}

static int hub_ext_port_status(struct usb_hub* hub, int port1, int type,
                               u16* status, u16* change, u32* ext_status)
{
    int retval;
    int len = 4;

    if (type != HUB_PORT_STATUS) len = 8;

    retval = get_port_status(hub->hdev, port1, &hub->status.port, type, len);
    if (retval < len) {
        if (retval >= 0)
            retval = EIO;
        else
            retval = -retval;
    } else {
        *status = hub->status.port.wPortStatus;
        *change = hub->status.port.wPortChange;
        if (type != HUB_PORT_STATUS && ext_status)
            *ext_status = hub->status.port.dwExtPortStatus;

        retval = 0;
    }

    return retval;
}

static int hub_port_status(struct usb_hub* hub, int port1, u16* status,
                           u16* change)
{
    return hub_ext_port_status(hub, port1, HUB_PORT_STATUS, status, change,
                               NULL);
}

static int hub_port_wait_reset(struct usb_hub* hub, int port1,
                               struct usb_device* udev, unsigned int delay)
{
    int retval, delay_time;
    u16 portstatus;
    u16 portchange;

    for (delay_time = 0; delay_time < HUB_RESET_TIMEOUT; delay_time += delay) {
        usleep(delay * 1000);

        retval = hub_port_status(hub, port1, &portstatus, &portchange);
        if (retval) return retval;

        if (!(portstatus & USB_PORT_STAT_RESET) &&
            (portstatus & USB_PORT_STAT_CONNECTION))
            break;

        if (delay_time >= 2 * HUB_SHORT_RESET_TIME) delay = HUB_LONG_RESET_TIME;
    }

    if (portstatus & USB_PORT_STAT_RESET) return EBUSY;

    if (!(portstatus & USB_PORT_STAT_CONNECTION)) return ENOTCONN;

    if (portchange & USB_PORT_STAT_C_CONNECTION) {
        usb_clear_port_feature(hub->hdev, port1, USB_PORT_FEAT_C_CONNECTION);
        return EAGAIN;
    }

    if (!(portstatus & USB_PORT_STAT_ENABLE)) return EBUSY;

    if (portstatus & USB_PORT_STAT_LOW_SPEED)
        udev->speed = USB_SPEED_LOW;
    else
        udev->speed = USB_SPEED_FULL;

    return 0;
}

static int hub_port_reset(struct usb_hub* hub, int port1,
                          struct usb_device* udev, unsigned int delay)
{
    struct usb_hcd* hcd = bus_to_hcd(hub->hdev->bus);
    int retval, i;

    for (i = 0; i < PORT_RESET_TRIES; i++) {
        retval = set_port_feature(hub->hdev, port1, USB_PORT_FEAT_RESET);
        if (retval < 0) retval = -retval;

        if (!retval) retval = hub_port_wait_reset(hub, port1, udev, delay);

        if (retval == 0 || retval == ENOTCONN || retval == ENODEV ||
            (retval == EBUSY && i == PORT_RESET_TRIES - 1)) {
            usb_clear_port_feature(hub->hdev, port1, USB_PORT_FEAT_C_RESET);

            goto done;
        }

        delay = HUB_LONG_RESET_TIME;
    }

done:
    if (!retval) {
        if (udev) {
            update_devnum(udev, 0);

            if (hcd->driver->reset_device) hcd->driver->reset_device(hcd, udev);
        }
    }

    return retval;
}

void usb_ep0_reinit(struct usb_device* udev)
{
    usb_disable_endpoint(udev, 0 + USB_DIR_IN, TRUE);
    usb_disable_endpoint(udev, 0 + USB_DIR_OUT, TRUE);
    usb_enable_endpoint(udev, &udev->ep0, TRUE);
}

#define usb_sndaddr0pipe() (PIPE_CONTROL << 30)
#define usb_rcvaddr0pipe() ((PIPE_CONTROL << 30) | USB_DIR_IN)

static int get_bMaxPacketSize0(struct usb_device* udev,
                               struct usb_device_descriptor* buf, int size)
{
    int i, retval;

    for (i = 0; i < GET_MAXPACKET0_TRIES; i++) {
        buf->bDescriptorType = buf->bMaxPacketSize0 = 0;
        retval =
            usb_control_msg(udev, usb_rcvaddr0pipe(), USB_REQ_GET_DESCRIPTOR,
                            USB_DIR_IN, USB_DT_DEVICE << 8, 0, buf, size);
        switch (buf->bMaxPacketSize0) {
        case 8:
        case 16:
        case 32:
        case 64:
        case 9:
            if (buf->bDescriptorType == USB_DT_DEVICE) {
                retval = buf->bMaxPacketSize0;
                break;
            }
        default:
            if (retval >= 0) retval = -EPROTO;
            break;
        }

        if (retval > 0) break;
    }

    return retval;
}

static int hub_set_address(struct usb_device* udev, int devnum)
{
    struct usb_hcd* hcd = bus_to_hcd(udev->bus);
    int retval;

    if (!hcd->driver->address_device && devnum <= 1) return -EINVAL;

    if (hcd->driver->address_device) {
        retval = hcd->driver->address_device(hcd, udev);
        if (retval) retval = -retval;
    } else
        retval = usb_control_msg(udev, usb_sndaddr0pipe(), USB_REQ_SET_ADDRESS,
                                 0, devnum, 0, NULL, 0);

    if (retval == 0) {
        update_devnum(udev, devnum);
        usb_ep0_reinit(udev);
    }

    return retval;
}

#define GET_DESCRIPTOR_BUFSIZE 64

static int hub_port_init(struct usb_hub* hub, struct usb_device* udev,
                         int port1, int retry_counter,
                         struct usb_device_descriptor* dev_descr)
{
    unsigned int delay = HUB_SHORT_RESET_TIME;
    int first = !dev_descr;
    int devnum = udev->devnum;
    enum usb_device_speed old_speed = udev->speed;
    const char* speed;
    const char* driver_name;
    int maxps;
    int retries, operations;
    struct usb_device_descriptor *buf, *descr;
    int retval;

    buf = malloc(GET_DESCRIPTOR_BUFSIZE);
    if (!buf) return ENOMEM;

    retval = hub_port_reset(hub, port1, udev, delay);
    if (retval) goto out;

    old_speed = udev->speed;

    if (first) {
        switch (udev->speed) {
        case USB_SPEED_FULL:
            udev->ep0.desc.wMaxPacketSize = 64;
            break;
        case USB_SPEED_LOW:
            udev->ep0.desc.wMaxPacketSize = 8;
            break;
        default:
            goto out;
        }
    }

    driver_name = bus_to_hcd(hub->hdev->bus)->driver->description;
    speed = usb_speed_string(udev->speed);

    if (udev->speed < USB_SPEED_SUPER) {
        printl("%s: %s %s USB device number %d using %s\n", name,
               (first ? "new" : "reset"), speed, devnum, driver_name);
    }

    for (retries = 0; retries < GET_DESCRIPTOR_TRIES;
         (++retries, usleep(100000))) {
        retval = hub_enable_device(udev);
        if (retval) goto out;

        maxps = get_bMaxPacketSize0(udev, buf, GET_DESCRIPTOR_BUFSIZE);
        if (maxps > 0 && !first && maxps != udev->descriptor.bMaxPacketSize0) {
            retval = ENODEV;
            goto out;
        }

        retval = hub_port_reset(hub, port1, udev, delay);
        if (retval) goto out;

        if (old_speed != udev->speed) {
            retval = ENODEV;
            goto out;
        }
        if (maxps < 0) {
            retval = -maxps;
            continue;
        }

        for (operations = 0; operations < SET_ADDRESS_TRIES; operations++) {
            retval = hub_set_address(udev, devnum);
            if (retval >= 0) break;
            usleep(200000);
        }
        if (retval < 0) {
            retval = -retval;
            goto out;
        }

        usleep(10000);
        break;
    }

    if (usb_endpoint_maxp(&udev->ep0.desc) == maxps) {
    } else if ((udev->speed == USB_SPEED_FULL ||
                udev->speed == USB_SPEED_HIGH) &&
               (maxps == 8 || maxps == 16 || maxps == 32 || maxps == 64)) {
        udev->ep0.desc.wMaxPacketSize = cpu_to_le16(maxps);
        usb_ep0_reinit(udev);
    } else {
        retval = EMSGSIZE;
        goto out;
    }

    descr = usb_get_device_descriptor(udev);
    if (!descr) {
        retval = EIO;
        goto out;
    }
    if (first)
        udev->descriptor = *descr;
    else
        *dev_descr = *descr;
    free(descr);

    retval = 0;

out:
    if (retval) {
        update_devnum(udev, devnum);
    }

    free(buf);
    return retval;
}

static void hub_port_connect(struct usb_hub* hub, int port1, u16 portstatus,
                             u16 portchange)
{
    struct usb_device* hdev = hub->hdev;
    struct usb_device* udev = hub->ports[port1 - 1];
    int retval, i;

    if (udev) {
        /* TODO: disconnect */
    }

    for (i = 0; i < PORT_INIT_TRIES; i++) {
        udev = usb_alloc_dev(hdev, hub->hdev->bus, port1);

        udev->speed = USB_SPEED_UNKNOWN;

        assign_devnum(udev);
        if (udev->devnum <= 0) {
            retval = ENOTCONN;
            goto again;
        }

        retval = hub_port_init(hub, udev, port1, i, NULL);
        if (retval) goto again;

        hub->ports[port1 - 1] = udev;

        retval = usb_new_device(udev);
        if (retval) goto again;

        return;

    again:
        usb_ep0_reinit(udev);
        release_devnum(udev);

        usb_put_dev(udev);

        if ((retval == ENOTCONN) || (retval == ENOTSUP)) break;
    }
}

static void hub_port_connect_change(struct usb_hub* hub, int port1,
                                    u16 portstatus, u16 portchange)
{
    hub_port_connect(hub, port1, portstatus, portchange);
}

static void port_event(struct usb_hub* hub, int port1)
{
    u16 portstatus, portchange;
    int connect_change = 0;
    int retval;

    UNSET_BIT(hub->event_bits, port1);

    retval = hub_port_status(hub, port1, &portstatus, &portchange);
    if (retval) return;

    if (portchange & USB_PORT_STAT_C_CONNECTION) {
        usb_clear_port_feature(hub->hdev, port1, USB_PORT_FEAT_C_CONNECTION);
        connect_change = 1;
    }

    if (portchange & USB_PORT_STAT_C_ENABLE) {
        usb_clear_port_feature(hub->hdev, port1, USB_PORT_FEAT_C_ENABLE);
    }

    if (portchange & USB_PORT_STAT_C_RESET) {
        usb_clear_port_feature(hub->hdev, port1, USB_PORT_FEAT_C_RESET);
    }

    if (connect_change)
        hub_port_connect_change(hub, port1, portstatus, portchange);
}

static void hub_event(struct usb_hub* hub)
{
    int i;

    for (i = 1; i <= hub->maxchild; i++) {
        if (GET_BIT(hub->event_bits, i)) {
            port_event(hub, i);
        }
    }
}

void usb_hub_handle_status_data(struct usb_hub* hub, const char* buffer,
                                int length)
{
    unsigned long bits;
    int i;

    bits = 0;
    for (i = 0; i < length; ++i)
        bits |= ((unsigned long)(buffer[i])) << (i * 8);
    hub->event_bits[0] = bits;

    hub_event(hub);
}

static void hub_irq(struct urb* urb) {}

static void announce_device(struct usb_device* udev)
{
    u16 bcdDevice = le16_to_cpu(udev->descriptor.bcdDevice);

    printl("%s: New USB device found, idVendor=%04x, idProduct=%04x, "
           "bcdDevice=%2x.%02x\n",
           name, le16_to_cpu(udev->descriptor.idVendor),
           le16_to_cpu(udev->descriptor.idProduct), bcdDevice >> 8,
           bcdDevice & 0xff);
    printl("%s: New USB device strings: Mfr=%d, Product=%d, SerialNumber=%d\n",
           name, udev->descriptor.iManufacturer, udev->descriptor.iProduct,
           udev->descriptor.iSerialNumber);

    if (udev->product) printl("%s: Product: %s\n", name, udev->product);
    if (udev->manufacturer)
        printl("%s: Manufacturer: %s\n", name, udev->manufacturer);
    if (udev->serial) printl("%s: SerialNumber: %s\n", name, udev->serial);
}

static int usb_enumerate_device(struct usb_device* udev)
{
    int retval;

    if (!udev->config) {
        retval = usb_get_configuration(udev);
        if (retval) return retval;
    }

    udev->product = usb_cache_string(udev, udev->descriptor.iProduct);
    udev->manufacturer = usb_cache_string(udev, udev->descriptor.iManufacturer);
    udev->serial = usb_cache_string(udev, udev->descriptor.iSerialNumber);

    return 0;
}

int usb_new_device(struct usb_device* udev)
{
    int retval;
    int cfg;

    retval = usb_enumerate_device(udev);
    if (retval) return retval;

    announce_device(udev);

    cfg = usb_choose_configuration(udev);
    if (cfg >= 0) usb_set_configuration(udev, cfg);

    retval = usb_register_device(udev);
    if (retval) return retval;

    return 0;
}
