#include <lyos/ipc.h>
#include <lyos/const.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libasyncdriver/libasyncdriver.h"

#include "usbhid.h"

struct urb_wait_context {
    async_worker_id_t wid;
    int done;
    int status;
};

void urb_helper_init_urb(struct usbhid_urb* urb, device_id_t dev_id,
                         struct urb_ep_config* ep_config)
{
    memset(urb, 0, sizeof(*urb));

    urb->dev_id = dev_id;
    urb->endpoint = ep_config->ep_num;
    urb->type = ep_config->type;
    urb->direction = ep_config->direction;
    urb->interval = ep_config->interval;
}

void urb_helper_attach_data(struct usbhid_urb* urb, int is_setup, void* data,
                            size_t len)
{
    assert(urb);
    assert(data);

    if (is_setup) {
        assert(len == sizeof(struct usb_ctrlrequest));
        urb->setup_packet = data;
    } else {
        urb->data = data;
        urb->size = len;
    }
}

static struct usb_urb* urb_to_libusb_urb(struct usbhid_urb* urb)
{
    unsigned int urb_size = USB_URBSIZE(urb->size, urb->number_of_packets);
    struct usb_urb* usb_urb;

    usb_urb = (struct usb_urb*)malloc(urb_size);
    if (usb_urb == NULL) return NULL;

    memset(usb_urb, 0, urb_size);
    usb_urb->urb_size = urb_size;

    usb_urb->dev_id = urb->dev_id;
    usb_urb->type = urb->type;
    usb_urb->endpoint = urb->endpoint;
    usb_urb->direction = urb->direction;
    usb_urb->interval = urb->interval;
    usb_urb->size = urb->size;

    if (urb->type == USB_TRANSFER_CTL) {
        memcpy(usb_urb->setup_packet, urb->setup_packet,
               sizeof(usb_urb->setup_packet));
    }

    if (urb->type == USB_TRANSFER_ISO) {
        usb_urb->number_of_packets = urb->number_of_packets;
        usb_urb->start_frame = urb->start_frame;
        usb_urb->iso_desc_offset = urb->size;
        memcpy(usb_urb->buffer + urb->size, urb->iso_desc,
               urb->number_of_packets *
                   sizeof(struct usb_iso_packet_descriptor));
    }

    memcpy(usb_urb->buffer, urb->data, urb->size);

    return usb_urb;
}

static void libusb_urb_to_urb(struct usbhid_urb* urb, struct usb_urb* usb_urb)
{
    urb->status = usb_urb->status;
    urb->error_count = usb_urb->error_count;
    urb->actual_length = usb_urb->actual_length;

    if (urb->type == USB_TRANSFER_CTL) {
        memcpy(urb->setup_packet, usb_urb->setup_packet,
               sizeof(usb_urb->setup_packet));
    }

    if (urb->type == USB_TRANSFER_ISO) {
        urb->start_frame = usb_urb->start_frame;
        memcpy(urb->iso_desc, usb_urb->buffer + urb->size,
               urb->number_of_packets *
                   sizeof(struct usb_iso_packet_descriptor));
    }

    memcpy(urb->data, usb_urb->buffer, urb->size);
}

int urb_helper_send_wait_urb(struct usbhid_urb* urb)
{
    struct urb_wait_context ctx;
    struct usb_urb* usb_urb = NULL;
    int retval;

    usb_urb = urb_to_libusb_urb(urb);
    if (usb_urb == NULL) return ENOMEM;

    ctx.wid = asyncdrv_worker_id();
    ctx.done = FALSE;
    usb_urb->priv = &ctx;
    usb_urb->actual_length = 0;

    retval = usb_send_urb(usb_urb);
    if (retval) goto out;

    if (!ctx.done) {
        asyncdrv_sleep();
    }

    libusb_urb_to_urb(urb, usb_urb);

out:
    free(usb_urb);
    return retval;
}

void urb_helper_complete_urb(struct usb_urb* urb)
{
    struct urb_wait_context* ctx = urb->priv;

    ctx->done = TRUE;
    ctx->status = urb->status;
    asyncdrv_wakeup(ctx->wid);
}
