#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "usb.h"
#include "hcd.h"
#include "worker.h"

struct urb_wait_context {
    worker_id_t wid;
    int done;
    int status;
};

static void usb_blocking_completion(struct urb* urb)
{
    struct urb_wait_context* ctx = urb->context;

    ctx->done = TRUE;
    ctx->status = urb->status;
    worker_async_wakeup(ctx->wid);
}

int usb_start_wait_urb(struct urb* urb, int* actual_length)
{
    struct urb_wait_context ctx;
    int retval;

    ctx.wid = current_worker_id();
    ctx.done = FALSE;
    urb->context = &ctx;
    urb->actual_length = 0;

    retval = usb_submit_urb(urb);
    if (retval) goto out;

    if (!ctx.done) {
        worker_async_sleep();
    }
    retval = ctx.status;

out:
    if (actual_length) *actual_length = urb->actual_length;

    usb_free_urb(urb);
    return retval;
}

int usb_internal_control_msg(struct usb_device* dev, unsigned int pipe,
                             struct usb_ctrlrequest* ctrl, void* data, u16 size)
{
    struct urb* urb;
    int length;
    int retval;

    urb = usb_alloc_urb(0);
    if (!urb) return -ENOMEM;

    usb_fill_control_urb(urb, dev, pipe, (unsigned char*)ctrl, data, size,
                         usb_blocking_completion, NULL);

    retval = usb_start_wait_urb(urb, &length);
    if (retval) return -retval;

    return length;
}

int usb_control_msg(struct usb_device* dev, unsigned int pipe, u8 request,
                    u8 requesttype, u16 value, u16 index, void* data, u16 size)
{
    struct usb_ctrlrequest* ctrl;
    int retval;

    ctrl = malloc(sizeof(*ctrl));
    if (!ctrl) return -ENOMEM;

    ctrl->bRequest = request;
    ctrl->bRequestType = requesttype;
    ctrl->wValue = value;
    ctrl->wIndex = index;
    ctrl->wLength = size;

    retval = usb_internal_control_msg(dev, pipe, ctrl, data, size);

    free(ctrl);
    return retval;
}

int usb_get_descriptor(struct usb_device* dev, unsigned char type,
                       unsigned char index, void* buf, int size)
{
    int i;
    int result;

    if (size <= 0) return -EINVAL;

    memset(buf, 0, size);

    for (i = 0; i < 3; ++i) {
        result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
                                 USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
                                 (type << 8) + index, 0, buf, size);

        if (result <= 0 && result != -ETIMEDOUT) continue;
        if (result > 1 && ((u8*)buf)[1] != type) {
            result = -ENODATA;
            continue;
        }
        break;
    }

    return result;
}

struct usb_device_descriptor* usb_get_device_descriptor(struct usb_device* udev)
{
    struct usb_device_descriptor* desc;
    int ret;

    desc = malloc(sizeof(*desc));
    if (!desc) return NULL;

    ret = usb_get_descriptor(udev, USB_DT_DEVICE, 0, desc, sizeof(*desc));
    if (ret == sizeof(*desc)) return desc;

    free(desc);
    return NULL;
}

void usb_enable_endpoint(struct usb_device* dev, struct usb_host_endpoint* ep,
                         int reset_ep)
{
    int epnum = usb_endpoint_num(&ep->desc);
    int is_out = usb_endpoint_dir_out(&ep->desc);
    int is_control = usb_endpoint_xfer_control(&ep->desc);

    if (reset_ep) usb_hcd_reset_endpoint(dev, ep);
    if (is_out || is_control) dev->ep_out[epnum] = ep;
    if (!is_out || is_control) dev->ep_in[epnum] = ep;
    ep->enabled = TRUE;
}

void usb_disable_endpoint(struct usb_device* dev, unsigned int epaddr,
                          int reset_hardware)
{
    unsigned int epnum = epaddr & USB_ENDPOINT_NUMBER_MASK;
    struct usb_host_endpoint* ep;

    if (!dev) return;

    if (!(epaddr & USB_DIR_IN)) {
        ep = dev->ep_out[epnum];
        if (reset_hardware && epnum != 0) dev->ep_out[epnum] = NULL;
    } else {
        ep = dev->ep_in[epnum];
        if (reset_hardware && epnum != 0) dev->ep_in[epnum] = NULL;
    }
    if (ep) {
        ep->enabled = 0;
        if (reset_hardware) usb_hcd_disable_endpoint(dev, ep);
    }
}
