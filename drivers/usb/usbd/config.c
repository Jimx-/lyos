#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "usb.h"

#define USB_MAXCONFIG     8
#define USB_MAXALTSETTING 128

static int find_next_descriptor(unsigned char* buffer, int size, int dt1,
                                int dt2, int* num_skipped)
{
    struct usb_descriptor_header* h;
    int n = 0;
    unsigned char* buffer0 = buffer;

    while (size > 0) {
        h = (struct usb_descriptor_header*)buffer;
        if (h->bDescriptorType == dt1 || h->bDescriptorType == dt2) break;
        buffer += h->bLength;
        size -= h->bLength;
        ++n;
    }

    if (num_skipped) *num_skipped = n;
    return buffer - buffer0;
}

static int usb_parse_endpoint(struct usb_device* dev, int cfgno,
                              struct usb_host_config* config, int inum,
                              int asnum, struct usb_host_interface* ifp,
                              int num_ep, unsigned char* buffer, int size)
{
    unsigned char* buffer0 = buffer;
    struct usb_endpoint_descriptor* d;
    struct usb_host_endpoint* endpoint;
    int i, n;

    d = (struct usb_endpoint_descriptor*)buffer;
    buffer += d->bLength;
    size -= d->bLength;

    if (d->bLength >= USB_DT_ENDPOINT_AUDIO_SIZE)
        n = USB_DT_ENDPOINT_AUDIO_SIZE;
    else if (d->bLength >= USB_DT_ENDPOINT_SIZE)
        n = USB_DT_ENDPOINT_SIZE;
    else
        goto skip;

    i = d->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
    if (i == 0) goto skip;

    if (ifp->desc.bNumEndpoints >= num_ep) goto skip;

    endpoint = &ifp->endpoint[ifp->desc.bNumEndpoints];
    memcpy(&endpoint->desc, d, n);
    d = &endpoint->desc;

    i = d->bEndpointAddress &
        (USB_ENDPOINT_DIR_MASK | USB_ENDPOINT_NUMBER_MASK);
    if (i != d->bEndpointAddress) endpoint->desc.bEndpointAddress = i;

    ++ifp->desc.bNumEndpoints;
    INIT_LIST_HEAD(&endpoint->urb_list);

    i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT, USB_DT_INTERFACE,
                             &n);
    return buffer - buffer0 + i;

skip:
    i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT, USB_DT_INTERFACE,
                             NULL);
    return buffer - buffer0 + i;
}

static int usb_parse_interface(struct usb_device* dev, int cfgno,
                               struct usb_host_config* config,
                               unsigned char* buffer, int size, u8 inums[],
                               u8 nalts[])
{
    unsigned char* buffer0 = buffer;
    struct usb_interface_descriptor* d;
    struct usb_interface_cache* intfc;
    struct usb_host_interface* alt;
    int inum, asnum;
    int num_ep, num_ep_orig;
    int i, n, retval;

    d = (struct usb_interface_descriptor*)buffer;
    buffer += d->bLength;
    size -= d->bLength;

    if (d->bLength < USB_DT_INTERFACE_SIZE) goto skip;

    intfc = NULL;
    inum = d->bInterfaceNumber;
    for (i = 0; i < config->desc.bNumInterfaces; ++i) {
        if (inums[i] == inum) {
            intfc = config->intf_cache[i];
            break;
        }
    }
    if (!intfc || intfc->num_altsetting >= nalts[i]) goto skip;

    asnum = d->bAlternateSetting;
    for (i = 0, alt = &intfc->altsetting[0]; i < intfc->num_altsetting;
         ++i, ++alt) {
        if (alt->desc.bAlternateSetting == asnum) goto skip;
    }

    ++intfc->num_altsetting;
    memcpy(&alt->desc, d, USB_DT_INTERFACE_SIZE);

    i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT, USB_DT_INTERFACE,
                             &n);
    buffer += i;
    size -= i;

    num_ep = num_ep_orig = alt->desc.bNumEndpoints;

    alt->desc.bNumEndpoints = 0;
    if (num_ep > USB_MAXENDPOINTS) num_ep = USB_MAXENDPOINTS;

    if (num_ep > 0) {
        alt->endpoint = calloc(num_ep, sizeof(struct usb_host_endpoint));
        if (!alt->endpoint) return -ENOMEM;
    }

    n = 0;
    while (size > 0) {
        if (((struct usb_descriptor_header*)buffer)->bDescriptorType ==
            USB_DT_INTERFACE)
            break;
        retval = usb_parse_endpoint(dev, cfgno, config, inum, asnum, alt,
                                    num_ep, buffer, size);
        if (retval < 0) return retval;
        ++n;

        buffer += retval;
        size -= retval;
    }

    return buffer - buffer0;

skip:
    i = find_next_descriptor(buffer, size, USB_DT_INTERFACE, USB_DT_INTERFACE,
                             NULL);
    return buffer - buffer0 + i;
}

static int usb_parse_configuration(struct usb_device* dev, int cfgidx,
                                   struct usb_host_config* config,
                                   unsigned char* buffer, int size)
{
    int cfgno;
    int nintf, nintf_orig;
    unsigned char* bp;
    struct usb_descriptor_header* header;
    int rem;
    int n, i, j;
    u8 inums[USB_MAXINTERFACES], nalts[USB_MAXINTERFACES];
    struct usb_interface_cache* intfc;
    int retval;

    memcpy(&config->desc, buffer, USB_DT_CONFIG_SIZE);
    nintf = nintf_orig = config->desc.bNumInterfaces;
    config->desc.bNumInterfaces = 0;

    if (config->desc.bDescriptorType != USB_DT_CONFIG ||
        config->desc.bLength < USB_DT_CONFIG_SIZE ||
        config->desc.bLength > size) {
        return EINVAL;
    }
    cfgno = config->desc.bConfigurationValue;

    buffer += config->desc.bLength;
    size -= config->desc.bLength;

    if (nintf > USB_MAXINTERFACES) nintf = USB_MAXINTERFACES;

    n = 0;
    for (bp = buffer, rem = size; rem > 0;
         bp += header->bLength, rem -= header->bLength) {
        if (rem < sizeof(struct usb_descriptor_header)) break;

        header = (struct usb_descriptor_header*)bp;

        if (header->bLength > rem || header->bLength < 2) break;

        if (header->bDescriptorType == USB_DT_INTERFACE) {
            struct usb_interface_descriptor* d;
            int inum;

            d = (struct usb_interface_descriptor*)header;
            if (d->bLength < USB_DT_INTERFACE_SIZE) continue;

            inum = d->bInterfaceNumber;

            for (i = 0; i < n; ++i) {
                if (inums[i] == inum) break;
            }
            if (i < n) {
                if (nalts[i] < 255) ++nalts[i];
            } else if (n < USB_MAXINTERFACES) {
                inums[n] = inum;
                nalts[n] = 1;
                ++n;
            }
        }
    }
    size = bp - buffer;
    config->desc.wTotalLength = cpu_to_le16(size);

    config->desc.bNumInterfaces = nintf = n;

    for (i = 0; i < nintf; ++i) {
        size_t alloc_size;

        j = nalts[i];
        if (j > USB_MAXALTSETTING) nalts[i] = j = USB_MAXALTSETTING;

        alloc_size = sizeof(*intfc) + sizeof(intfc->altsetting[0]) * j;
        intfc = malloc(alloc_size);
        config->intf_cache[i] = intfc;
        if (!intfc) return ENOMEM;

        memset(intfc, 0, alloc_size);
        kref_init(&intfc->kref);
    }

    i = find_next_descriptor(buffer, size, USB_DT_INTERFACE, USB_DT_INTERFACE,
                             &n);
    buffer += i;
    size -= i;

    while (size > 0) {
        retval =
            usb_parse_interface(dev, cfgno, config, buffer, size, inums, nalts);
        if (retval < 0) return -retval;

        buffer += retval;
        size -= retval;
    }

    return 0;
}

int usb_get_configuration(struct usb_device* udev)
{
    int ncfg = udev->descriptor.bNumConfigurations;
    int cfgno;
    struct usb_config_descriptor* desc;
    char* buffer;
    int length;
    int retval;

    if (ncfg > USB_MAXCONFIG) {
        ncfg = udev->descriptor.bNumConfigurations = USB_MAXCONFIG;
    }

    if (ncfg < 1) return EINVAL;

    udev->config = calloc(ncfg, sizeof(struct usb_host_config));
    if (!udev->config) return ENOMEM;

    udev->raw_descr = calloc(ncfg, sizeof(char*));
    if (!udev->raw_descr) return ENOMEM;

    desc = malloc(USB_DT_CONFIG_SIZE);
    if (!desc) return ENOMEM;

    for (cfgno = 0; cfgno < ncfg; cfgno++) {
        retval = usb_get_descriptor(udev, USB_DT_CONFIG, cfgno, desc,
                                    USB_DT_CONFIG_SIZE);
        if (retval < 0) {
            if (retval != -EPIPE) {
                retval = -retval;
                goto out;
            }

            udev->descriptor.bNumConfigurations = cfgno;
            break;
        } else if (retval < 4) {
            retval = EINVAL;
            goto out;
        }

        length = max((int)le16_to_cpu(desc->wTotalLength), USB_DT_CONFIG_SIZE);

        buffer = malloc(length);
        if (!buffer) {
            retval = ENOMEM;
            goto out;
        }

        retval = usb_get_descriptor(udev, USB_DT_CONFIG, cfgno, buffer, length);
        if (retval < 0) {
            free(buffer);
            retval = -retval;
            goto out;
        }

        if (retval < length) length = retval;

        udev->raw_descr[cfgno] = buffer;

        retval = usb_parse_configuration(udev, cfgno, &udev->config[cfgno],
                                         (unsigned char*)buffer, length);
        if (retval) {
            cfgno++;
            goto out;
        }
    }

out:
    free(desc);
    udev->descriptor.bNumConfigurations = cfgno;

    return 0;
}

int usb_choose_configuration(struct usb_device* udev)
{
    int i;
    int num_configs;
    struct usb_host_config *c, *best;

    best = NULL;
    num_configs = udev->descriptor.bNumConfigurations;
    for (i = 0, c = udev->config; i < num_configs; i++, c++) {
        struct usb_interface_descriptor* desc = NULL;

        if (c->desc.bNumInterfaces > 0)
            desc = &c->intf_cache[0]->altsetting->desc;

        if (udev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC &&
            (desc && desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC)) {
            best = c;
            break;
        } else if (!best)
            best = c;
    }

    if (best) return best->desc.bConfigurationValue;

    return -1;
}

int usb_set_configuration(struct usb_device* dev, int configuration)
{
    int i, retval;
    struct usb_host_config* cp = NULL;
    struct usb_interface** new_interfaces = NULL;
    int n, nintf;

    if (configuration == -1)
        configuration = 0;
    else {
        for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
            if (dev->config[i].desc.bConfigurationValue == configuration) {
                cp = &dev->config[i];
                break;
            }
        }
    }
    if ((!cp && configuration != 0)) return EINVAL;

    n = nintf = 0;
    if (cp) {
        nintf = cp->desc.bNumInterfaces;
        new_interfaces = calloc(nintf, sizeof(*new_interfaces));
        if (!new_interfaces) return ENOMEM;

        for (; n < nintf; ++n) {
            new_interfaces[n] = malloc(sizeof(struct usb_interface));
            if (!new_interfaces[n]) {
                retval = ENOMEM;

                while (--n >= 0)
                    free(new_interfaces[n]);
                free(new_interfaces);
                return retval;
            }

            memset(new_interfaces[n], 0, sizeof(struct usb_interface));
        }
    }

    for (i = 0; i < nintf; ++i) {
        struct usb_interface_cache* intfc;
        struct usb_interface* intf;
        struct usb_host_interface* alt;

        cp->interface[i] = intf = new_interfaces[i];
        intfc = cp->intf_cache[i];
        intf->altsetting = intfc->altsetting;
        intf->num_altsetting = intfc->num_altsetting;

        kref_get(&intfc->kref);

        alt = usb_altnum_to_altsetting(intf, 0);
        if (!alt) alt = &intf->altsetting[0];

        kref_init(&intf->kref);
        intf->cur_altsetting = alt;
        usb_enable_interface(dev, intf, TRUE);
    }
    free(new_interfaces);

    retval = usb_control_msg_send(dev, 0, USB_REQ_SET_CONFIGURATION, 0,
                                  configuration, 0, NULL, 0);
    if (retval && cp) {
        for (i = 0; i < nintf; ++i) {
            usb_disable_interface(dev, cp->interface[i], TRUE);
            usb_put_interface(cp->interface[i]);
            cp->interface[i] = NULL;
        }
        cp = NULL;
    }

    dev->actconfig = cp;

    if (cp->string == NULL)
        cp->string = usb_cache_string(dev, cp->desc.iConfiguration);

    return 0;
}
