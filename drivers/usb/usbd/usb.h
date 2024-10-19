#ifndef _USBD_USB_H_
#define _USBD_USB_H_

#include <lyos/bitmap.h>
#include <lyos/kref.h>
#include <lyos/list.h>
#include <lyos/usb.h>

struct usb_device;
struct usb_hub;

#define USB_MAXCHILDREN 31

struct usb_host_endpoint {
    struct usb_endpoint_descriptor desc;
    struct list_head urb_list;
    void* hcpriv;

    int enabled;
};

struct usb_host_interface {
    struct usb_interface_descriptor desc;

    struct usb_host_endpoint* endpoint;

    char* string;
};

struct usb_interface {
    struct kref kref;

    struct usb_host_interface* altsetting;
    struct usb_host_interface* cur_altsetting;

    unsigned int num_altsetting;
};

struct usb_interface* usb_get_interface(struct usb_interface* intf);
void usb_put_interface(struct usb_interface* intf);

struct usb_interface_cache {
    struct kref kref;
    int num_altsetting;

    struct usb_host_interface altsetting[0];
};

#define USB_MAXENDPOINTS  30
#define USB_MAXINTERFACES 32

struct usb_host_config {
    struct usb_config_descriptor desc;

    struct usb_interface* interface[USB_MAXINTERFACES];

    struct usb_interface_cache* intf_cache[USB_MAXINTERFACES];

    char* string;
};

struct usb_host_interface*
usb_altnum_to_altsetting(const struct usb_interface* intf, unsigned int altnum);

struct usb_bus {
    int busnum;

    int devnum_next;
    bitchunk_t devmap[BITCHUNKS(128)];

    struct usb_hub* roothub;
};

struct usb_device {
    struct kref kref;
    struct usb_bus* bus;
    struct usb_device* parent;
    int devnum;
    enum usb_device_speed speed;

    unsigned int toggle[2];

    struct usb_device_descriptor descriptor;
    struct usb_host_config* config;

    char** raw_descr;

    struct usb_host_config* actconfig;

    struct usb_host_endpoint ep0;
    struct usb_host_endpoint* ep_in[16];
    struct usb_host_endpoint* ep_out[16];

    int portnum;
    int devaddr;

    unsigned int has_langid;
    int string_langid;

    char* product;
    char* manufacturer;
    char* serial;
};

#define usb_gettoggle(dev, ep, out) (((dev)->toggle[out] >> (ep)) & 1)
#define usb_dotoggle(dev, ep, out)  ((dev)->toggle[out] ^= (1 << (ep)))
#define usb_settoggle(dev, ep, out, bit) \
    ((dev)->toggle[out] = ((dev)->toggle[out] & ~(1 << (ep))) | ((bit) << (ep)))

struct usb_device* usb_alloc_dev(struct usb_device* parent, struct usb_bus* bus,
                                 unsigned int port1);
struct usb_device* usb_get_dev(struct usb_device* udev);
void usb_put_dev(struct usb_device* udev);

const char* usb_speed_string(enum usb_device_speed speed);

struct usb_iso_packet_descriptor {
    unsigned int offset;
    unsigned int length;
    unsigned int actual_length;
    int status;
};

struct urb;

typedef void (*usb_complete_t)(struct urb*);

struct urb {
    struct kref kref;
    int unlinked;
    void* hc_priv;

    struct list_head urb_list;

    struct usb_device* dev;
    struct usb_host_endpoint* ep;
    unsigned int pipe;

    int status;
    unsigned int transfer_flags;
#define URB_FREE_BUFFER  0x0001
#define URB_NO_INTERRUPT 0x0002
#define URB_SHORT_NOT_OK 0x0004
#define URB_DIR_IN       0x0200
#define URB_DIR_OUT      0
#define URB_DIR_MASK     URB_DIR_IN

    void* transfer_buffer;
    phys_bytes transfer_phys;
    u32 transfer_buffer_length;
    u32 actual_length;

    unsigned char* setup_packet;
    phys_bytes setup_phys;

    int start_frame;
    int num_packets;
    int interval;

    int err_count;

    void* context;
    usb_complete_t complete;

    struct usb_iso_packet_descriptor iso_desc[0];
};

struct urb* usb_alloc_urb(int iso_packets);
struct urb* usb_get_urb(struct urb* urb);
void usb_free_urb(struct urb* urb);
#define usb_put_urb usb_free_urb
int usb_submit_urb(struct urb* urb);

static inline void
usb_fill_control_urb(struct urb* urb, struct usb_device* dev, unsigned int pipe,
                     unsigned char* setup_packet, void* transfer_buffer,
                     int buffer_length, usb_complete_t complete_fn,
                     void* context)
{
    urb->dev = dev;
    urb->pipe = pipe;
    urb->setup_packet = setup_packet;
    urb->transfer_buffer = transfer_buffer;
    urb->transfer_buffer_length = buffer_length;
    urb->complete = complete_fn;
    urb->context = context;
}

static inline void usb_fill_int_urb(struct urb* urb, struct usb_device* dev,
                                    unsigned int pipe, void* transfer_buffer,
                                    int buffer_length,
                                    usb_complete_t complete_fn, void* context,
                                    int interval)
{
    urb->dev = dev;
    urb->pipe = pipe;
    urb->transfer_buffer = transfer_buffer;
    urb->transfer_buffer_length = buffer_length;
    urb->complete = complete_fn;
    urb->context = context;
    urb->interval = interval;
    urb->start_frame = -1;
}

#define PIPE_ISOCHRONOUS 0
#define PIPE_INTERRUPT   1
#define PIPE_CONTROL     2
#define PIPE_BULK        3

#define usb_pipein(pipe)  ((pipe) & USB_DIR_IN)
#define usb_pipeout(pipe) (!usb_pipein(pipe))

#define usb_pipedevice(pipe)   (((pipe) >> 8) & 0x7f)
#define usb_pipeendpoint(pipe) (((pipe) >> 15) & 0xf)

#define usb_pipetype(pipe)    (((pipe) >> 30) & 3)
#define usb_pipeisoc(pipe)    (usb_pipetype((pipe)) == PIPE_ISOCHRONOUS)
#define usb_pipeint(pipe)     (usb_pipetype((pipe)) == PIPE_INTERRUPT)
#define usb_pipecontrol(pipe) (usb_pipetype((pipe)) == PIPE_CONTROL)
#define usb_pipebulk(pipe)    (usb_pipetype((pipe)) == PIPE_BULK)

static inline unsigned int __create_pipe(struct usb_device* dev,
                                         unsigned int endpoint)
{
    return (dev->devnum << 8) | (endpoint << 15);
}

#define usb_sndctrlpipe(dev, endpoint) \
    ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint))
#define usb_rcvctrlpipe(dev, endpoint) \
    ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndisocpipe(dev, endpoint) \
    ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint))
#define usb_rcvisocpipe(dev, endpoint) \
    ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndbulkpipe(dev, endpoint) \
    ((PIPE_BULK << 30) | __create_pipe(dev, endpoint))
#define usb_rcvbulkpipe(dev, endpoint) \
    ((PIPE_BULK << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndintpipe(dev, endpoint) \
    ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint))
#define usb_rcvintpipe(dev, endpoint) \
    ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

static inline struct usb_host_endpoint*
usb_pipe_endpoint(struct usb_device* dev, unsigned int pipe)
{
    struct usb_host_endpoint** eps;
    eps = usb_pipein(pipe) ? dev->ep_in : dev->ep_out;
    return eps[usb_pipeendpoint(pipe)];
}

void usb_enable_endpoint(struct usb_device* dev, struct usb_host_endpoint* ep,
                         int reset_ep);
void usb_disable_endpoint(struct usb_device* dev, unsigned int epaddr,
                          int reset_hardware);
void usb_enable_interface(struct usb_device* dev, struct usb_interface* intf,
                          int reset_eps);
void usb_disable_interface(struct usb_device* dev, struct usb_interface* intf,
                           int reset_hardware);

int usb_control_msg(struct usb_device* dev, unsigned int pipe, u8 request,
                    u8 requesttype, u16 value, u16 index, void* data, u16 size);
int usb_control_msg_send(struct usb_device* dev, u8 endpoint, u8 request,
                         u8 requesttype, u16 value, u16 index,
                         const void* driver_data, u16 size);

int usb_get_descriptor(struct usb_device* dev, unsigned char type,
                       unsigned char index, void* buf, int size);
struct usb_device_descriptor*
usb_get_device_descriptor(struct usb_device* udev);

int usb_get_configuration(struct usb_device* dev);
int usb_choose_configuration(struct usb_device* udev);
int usb_set_configuration(struct usb_device* dev, int configuration);

char* usb_cache_string(struct usb_device* udev, int index);

#endif
