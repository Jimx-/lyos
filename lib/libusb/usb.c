#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>
#include "libsysfs/libsysfs.h"
#include "libdevman/libdevman.h"

#include "libusb.h"

static endpoint_t __usbd_endpoint = NO_TASK;
static struct usb_urb* pending_urbs = NULL;

static int usbd_sendrec(int function, MESSAGE* msg)
{
    int retval;
    u32 v;

    if (__usbd_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.usbd.endpoint", &v);
        if (retval) return retval;

        __usbd_endpoint = (endpoint_t)v;
    }

    return send_recv(function, __usbd_endpoint, msg);
}

int usb_init(void)
{
    MESSAGE msg;
    int retval;

    msg.type = USB_RQ_INIT;
    retval = usbd_sendrec(BOTH, &msg);
    if (retval) return retval;

    if (msg.u.m_usb_reply.status) {
        panic("usb_init: init failed %d", msg.RETVAL);
    }

    return 0;
}

int usb_send_urb(struct usb_urb* urb)
{
    MESSAGE msg;
    mgrant_id_t grant;
    int retval;

    if (urb == NULL) return EINVAL;
    if (__usbd_endpoint == NO_TASK) return EINVAL;

    grant =
        mgrant_set_direct(__usbd_endpoint, (vir_bytes)&urb->dev_id,
                          urb->urb_size - sizeof(void*), MGF_READ | MGF_WRITE);
    if (grant == GRANT_INVALID) return EINVAL;

    urb->grant = grant;

    msg.type = USB_RQ_SEND_URB;
    msg.u.m_usb_send_urb.grant = grant;
    msg.u.m_usb_send_urb.grant_size = urb->urb_size - sizeof(void*);

    retval = usbd_sendrec(BOTH, &msg);
    if (retval) return retval;

    if (msg.type != USB_REPLY) {
        panic("usb_send_urb: got invalid reply from usbd: %d", msg.type);
    }

    if (msg.u.m_usb_reply.status) return msg.u.m_usb_reply.status;

    urb->urb_id = msg.u.m_usb_reply.urb_id;
    urb->next = pending_urbs;
    pending_urbs = urb;

    return 0;
}

static void _usb_complete_urb(const struct libusb_driver* ud,
                              const MESSAGE* msg)
{
    int status = msg->u.m_usb_reply.status;
    unsigned int urb_id = msg->u.m_usb_reply.urb_id;
    struct usb_urb* urb = NULL;

    if (pending_urbs != NULL) {
        if (pending_urbs->urb_id == urb_id) {
            urb = pending_urbs;
            pending_urbs = urb->next;
        } else {
            struct usb_urb* u = pending_urbs;
            while (u->next) {
                if (u->next->urb_id == urb_id) {
                    urb = u->next;
                    u->next = u->next->next;
                    urb->next = NULL;
                    break;
                }
                u = u->next;
            }
        }
    }

    if (urb != NULL) {
        if (status != 0) urb->status = status;

        mgrant_revoke(urb->grant);
        ud->complete_urb(urb);
    }
}

void usb_handle_message(const struct libusb_driver* ud, const MESSAGE* msg)
{
    if (!ud) return;

    switch (msg->type) {
    case USB_DEVICE_CONNECT:
        ud->connect_device(msg->DEVICE, msg->MASK);
        break;
    case USB_RQ_COMPLETE_URB:
        _usb_complete_urb(ud, msg);
        break;
    }
}
