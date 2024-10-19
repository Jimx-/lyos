#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdio.h>
#include <errno.h>

#include <libsysfs/libsysfs.h>

#include "usb.h"

#define usb_actconfig_show(field, format)                                  \
    static ssize_t field##_show(struct device_attribute* attr, char* buf)  \
    {                                                                      \
        struct usb_device* udev = attr->cb_data;                           \
        struct usb_host_config* actconfig = udev->actconfig;               \
        if (actconfig) return sprintf(buf, format, actconfig->desc.field); \
        return 0;                                                          \
    }

usb_actconfig_show(bNumInterfaces, "%2d\n");
usb_actconfig_show(bmAttributes, "%2x\n");

static ssize_t configuration_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    struct usb_host_config* actconfig = udev->actconfig;

    if (actconfig && actconfig->string)
        return sprintf(buf, "%s\n", actconfig->string);

    return 0;
}

usb_actconfig_show(bConfigurationValue, "%u\n");

static ssize_t bConfigurationValue_store(struct device_attribute* attr,
                                         const char* buf, size_t count)
{
    struct usb_device* udev = attr->cb_data;
    int config, value;

    if (sscanf(buf, "%d", &config) != 1 || config < -1 || config > 255)
        return -EINVAL;

    value = usb_set_configuration(udev, config);

    return (value < 0) ? value : count;
}

#define usb_descriptor_attr_le16(field, format_string)                    \
    static ssize_t field##_show(struct device_attribute* attr, char* buf) \
    {                                                                     \
        struct usb_device* udev = attr->cb_data;                          \
        return sprintf(buf, format_string,                                \
                       le16_to_cpu(udev->descriptor.field));              \
    }

usb_descriptor_attr_le16(idVendor, "%04x\n");
usb_descriptor_attr_le16(idProduct, "%04x\n");
usb_descriptor_attr_le16(bcdDevice, "%04x\n");

#define usb_descriptor_attr(field, format_string)                         \
    static ssize_t field##_show(struct device_attribute* attr, char* buf) \
    {                                                                     \
        struct usb_device* udev = attr->cb_data;                          \
        return sprintf(buf, format_string, udev->descriptor.field);       \
    }

usb_descriptor_attr(bDeviceClass, "%02x\n");
usb_descriptor_attr(bDeviceSubClass, "%02x\n");
usb_descriptor_attr(bDeviceProtocol, "%02x\n");
usb_descriptor_attr(bNumConfigurations, "%d\n");
usb_descriptor_attr(bMaxPacketSize0, "%d\n");

#define usb_string_attr(name)                                            \
    static ssize_t name##_show(struct device_attribute* attr, char* buf) \
    {                                                                    \
        struct usb_device* udev = attr->cb_data;                         \
        return sprintf(buf, "%s\n", udev->name);                         \
    }

usb_string_attr(product);
usb_string_attr(manufacturer);
usb_string_attr(serial);

static ssize_t speed_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    char* speed;

    switch (udev->speed) {
    case USB_SPEED_LOW:
        speed = "1.5";
        break;
    case USB_SPEED_UNKNOWN:
    case USB_SPEED_FULL:
        speed = "12";
        break;
    case USB_SPEED_HIGH:
        speed = "480";
        break;
    case USB_SPEED_SUPER:
        speed = "5000";
        break;
    case USB_SPEED_SUPER_PLUS:
        speed = "10000";
        break;
    default:
        speed = "unknown";
        break;
    }

    return sprintf(buf, "%s\n", speed);
}

static ssize_t busnum_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    return sprintf(buf, "%d\n", udev->bus->busnum);
}

static ssize_t devnum_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    return sprintf(buf, "%d\n", udev->devnum);
}

static ssize_t devpath_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    return sprintf(buf, "%s\n", udev->devpath);
}

static ssize_t version_show(struct device_attribute* attr, char* buf)
{
    struct usb_device* udev = attr->cb_data;
    u16 bcdUSB;

    bcdUSB = le16_to_cpu(udev->descriptor.bcdUSB);
    return sprintf(buf, "%2x.%02x\n", bcdUSB >> 8, bcdUSB & 0xff);
}

int usb_create_sysfs_dev_files(struct usb_device* udev)
{
    struct device_attribute attr;

    dm_init_device_attr(&attr, udev->dev_id, "configuration", SF_PRIV_OVERWRITE,
                        udev, configuration_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bNumInterfaces",
                        SF_PRIV_OVERWRITE, udev, bNumInterfaces_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bConfigurationValue",
                        SF_PRIV_OVERWRITE, udev, bConfigurationValue_show,
                        bConfigurationValue_store);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bmAttributes", SF_PRIV_OVERWRITE,
                        udev, bmAttributes_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "idVendor", SF_PRIV_OVERWRITE,
                        udev, idVendor_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "idProduct", SF_PRIV_OVERWRITE,
                        udev, idProduct_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bcdDevice", SF_PRIV_OVERWRITE,
                        udev, bcdDevice_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bDeviceClass", SF_PRIV_OVERWRITE,
                        udev, bDeviceClass_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bDeviceSubClass",
                        SF_PRIV_OVERWRITE, udev, bDeviceSubClass_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bDeviceProtocol",
                        SF_PRIV_OVERWRITE, udev, bDeviceProtocol_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bNumConfigurations",
                        SF_PRIV_OVERWRITE, udev, bNumConfigurations_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "bMaxPacketSize0",
                        SF_PRIV_OVERWRITE, udev, bMaxPacketSize0_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "speed", SF_PRIV_OVERWRITE, udev,
                        speed_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "busnum", SF_PRIV_OVERWRITE, udev,
                        busnum_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "devnum", SF_PRIV_OVERWRITE, udev,
                        devnum_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "devpath", SF_PRIV_OVERWRITE, udev,
                        devpath_show, NULL);
    dm_device_attr_add(&attr);

    dm_init_device_attr(&attr, udev->dev_id, "version", SF_PRIV_OVERWRITE, udev,
                        version_show, NULL);
    dm_device_attr_add(&attr);
    if (udev->product) {
        dm_init_device_attr(&attr, udev->dev_id, "product", SF_PRIV_OVERWRITE,
                            udev, product_show, NULL);
        dm_device_attr_add(&attr);
    }

    if (udev->manufacturer) {
        dm_init_device_attr(&attr, udev->dev_id, "manufacturer",
                            SF_PRIV_OVERWRITE, udev, manufacturer_show, NULL);
        dm_device_attr_add(&attr);
    }

    if (udev->serial) {
        dm_init_device_attr(&attr, udev->dev_id, "serial", SF_PRIV_OVERWRITE,
                            udev, serial_show, NULL);
        dm_device_attr_add(&attr);
    }

    return 0;
}
