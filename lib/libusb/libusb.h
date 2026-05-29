#ifndef _LIBUSB_H_
#define _LIBUSB_H_

#include <lyos/types.h>
#include <lyos/usb.h>

#define USB_URBSIZE(data_size, iso_count) \
    (data_size + sizeof(struct usb_urb) + \
     iso_count * sizeof(struct usb_iso_packet_descriptor))

#define USB_PREPARE_URB(urb, data_size, iso_count)                            \
    do {                                                                      \
        if (iso_count) urb->iso_data.iso_desc = data_size;                    \
        urb->urb_size = data_size + sizeof(struct usb_urb) +                  \
                        iso_count * sizeof(struct usb_iso_packet_descriptor); \
    } while (0)

struct usb_urb;

struct libusb_driver {
    void (*complete_urb)(struct usb_urb* urb);
    void (*connect_device)(device_id_t dev_id, unsigned int interfaces);
};

/* Transfer types */
#define USB_TRANSFER_ISO 0
#define USB_TRANSFER_INT 1
#define USB_TRANSFER_CTL 2
#define USB_TRANSFER_BLK 3

/* Transfer direction */
#define USB_IN  0
#define USB_OUT 1

struct usb_urb {
    struct usb_urb* next;

    device_id_t dev_id;
    int type;
    int endpoint;
    int direction;
    int status;
    int error_count;
    size_t size;
    size_t actual_length;
    void* priv;
    int interval;

    unsigned long urb_id;
    size_t urb_size;
    mgrant_id_t grant;

    size_t iso_desc_offset;
    int number_of_packets;
    int start_frame;
    unsigned char setup_packet[8];

    char buffer[0];
};

int usb_init(void);

int usb_send_urb(struct usb_urb* urb);

void usb_handle_message(const struct libusb_driver* ud, const MESSAGE* msg);

#endif
