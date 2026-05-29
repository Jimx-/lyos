#ifndef _USBHID_USBHID_H_
#define _USBHID_USBHID_H_

#include <lyos/types.h>
#include <lyos/input.h>
#include <uapi/linux/hid.h>
#include "libdevman/libdevman.h"
#include "libusb/libusb.h"
#include "libinputdriver/libinputdriver.h"

#define URB_INVALID_EP (-1)

struct urb_ep_config {
    int ep_num;
    int type;
    int direction;
    int max_packet_size;
    int interval;
};

struct hid_item {
    unsigned format;
#define HID_ITEM_FORMAT_SHORT 0
#define HID_ITEM_FORMAT_LONG  1

    __u8 size;
    __u8 type;
    __u8 tag;
#define HID_ITEM_TAG_LONG 15

    union {
        __u8 u8;
        __s8 s8;
        __u16 u16;
        __s16 s16;
        __u32 u32;
        __s32 s32;
        const __u8* longdata;
    } data;
};

#define HID_MAIN_ITEM_TAG_INPUT            8
#define HID_MAIN_ITEM_TAG_OUTPUT           9
#define HID_MAIN_ITEM_TAG_FEATURE          11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION 10
#define HID_MAIN_ITEM_TAG_END_COLLECTION   12

#define HID_MAIN_ITEM_CONSTANT      0x001
#define HID_MAIN_ITEM_VARIABLE      0x002
#define HID_MAIN_ITEM_RELATIVE      0x004
#define HID_MAIN_ITEM_WRAP          0x008
#define HID_MAIN_ITEM_NONLINEAR     0x010
#define HID_MAIN_ITEM_NO_PREFERRED  0x020
#define HID_MAIN_ITEM_NULL_STATE    0x040
#define HID_MAIN_ITEM_VOLATILE      0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE 0x100

/* HID report descriptor collection item types */
#define HID_COLLECTION_PHYSICAL    0
#define HID_COLLECTION_APPLICATION 1
#define HID_COLLECTION_LOGICAL     2
#define HID_COLLECTION_NAMED_ARRAY 4

/* HID report descriptor global item types */
#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE       0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM  1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM  2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM 3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM 4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT    5
#define HID_GLOBAL_ITEM_TAG_UNIT             6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE      7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID        8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT     9
#define HID_GLOBAL_ITEM_TAG_PUSH             10
#define HID_GLOBAL_ITEM_TAG_POP              11

/* HID report descriptor local item types */
#define HID_LOCAL_ITEM_TAG_USAGE              0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM      1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM      2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX   3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM 4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM 5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX       7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM     8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM     9
#define HID_LOCAL_ITEM_TAG_DELIMITER          10

/* HID usage tables */

#define HID_USAGE_PAGE 0xffff0000

#define HID_UP_KEYBOARD 0x00070000

#define HID_USAGE 0x0000ffff

struct hid_class_descriptor {
    __u8 bDescriptorType;
    __le16 wDescriptorLength;
} __attribute__((packed));

struct hid_descriptor {
    __u8 bLength;
    __u8 bDescriptorType;
    __le16 bcdHID;
    __u8 bCountryCode;
    __u8 bNumDescriptors;

    struct hid_class_descriptor desc[1];
} __attribute__((packed));

struct hid_global {
    unsigned usage_page;
    __s32 logical_minimum;
    __s32 logical_maximum;
    __s32 physical_minimum;
    __s32 physical_maximum;
    __s32 unit_exponent;
    unsigned unit;
    unsigned report_id;
    unsigned report_size;
    unsigned report_count;
};

#define HID_MAX_USAGES              12288
#define HID_DEFAULT_NUM_COLLECTIONS 16

struct hid_local {
    unsigned usage[HID_MAX_USAGES];
    u8 usage_size[HID_MAX_USAGES];
    unsigned collection_index[HID_MAX_USAGES];
    unsigned usage_index;
    unsigned usage_minimum;
    unsigned delimiter_depth;
    unsigned delimiter_branch;
};

struct hid_collection {
    int parent_idx;
    unsigned type;
    unsigned usage;
    unsigned level;
};

struct hid_usage {
    unsigned hid;
    unsigned collection_index;
    unsigned usage_index;
    __s8 resolution_multiplier;
    __s8 wheel_factor;
    __u16 code;
    __u8 type;
    __s16 hat_min;
    __s16 hat_max;
    __s16 hat_dir;
    __s16 wheel_accumulated;
};

struct hid_field {
    unsigned physical;
    unsigned logical;
    unsigned application;
    struct hid_usage* usage;
    unsigned maxusage;
    unsigned flags;
    unsigned report_offset;
    unsigned report_size;
    unsigned report_count;
    unsigned report_type;
    __s32* value;
    __s32* new_value;
    __s32* usages_priorities;
    __s32 logical_minimum;
    __s32 logical_maximum;
    __s32 physical_minimum;
    __s32 physical_maximum;
    __s32 unit_exponent;
    unsigned unit;
    int ignored;
    struct hid_report* report;
    unsigned index;
    unsigned int slot_idx;
};

#define HID_MAX_FIELDS 256

struct hid_field_entry {
    struct list_head list;
    struct hid_field* field;
    unsigned int index;
    __s32 priority;
};

struct hid_report {
    struct list_head list;
    struct list_head hidinput_list;
    struct list_head field_entry_list;
    unsigned int id;
    enum hid_report_type type;
    unsigned int application;
    struct hid_field* field[HID_MAX_FIELDS];
    struct hid_field_entry* field_entries;
    unsigned maxfield;
    unsigned size;
    struct usbhid_device* device;
};

#define HID_MAX_IDS 256

struct hid_report_enum {
    unsigned numbered;
    struct list_head report_list;
    struct hid_report* report_id_hash[HID_MAX_IDS];
};

#define HID_MAX_BUFFER_SIZE 16384

struct usbhid_device {
    device_id_t dev_id;
    unsigned int interfaces;
    int running;

    struct inputdriver_dev input_dev;

    struct urb_ep_config ep_in;
    struct urb_ep_config ep_out;

    struct hid_collection* collection;
    unsigned int collection_size;
    unsigned int maxcollection;
    unsigned int maxapplication;

    struct hid_report_enum report_enum[HID_REPORT_TYPES];
};

struct usbhid_urb {
    device_id_t dev_id;
    int type;
    int endpoint;
    int direction;
    int status;
    int error_count;
    size_t size;
    size_t actual_length;
    int interval;

    int number_of_packets;
    int start_frame;
    char* setup_packet;
    char* data;
    struct usb_iso_packet_descriptor* iso_desc;
};

void urb_helper_init_urb(struct usbhid_urb* urb, device_id_t dev_id,
                         struct urb_ep_config* ep_config);
void urb_helper_attach_data(struct usbhid_urb* urb, int is_setup, void* data,
                            size_t len);
int urb_helper_send_wait_urb(struct usbhid_urb* urb);
void urb_helper_complete_urb(struct usb_urb* urb);

int hid_parse_descriptor(struct usbhid_device* usbhid_dev, const u8* buf,
                         int size);

#endif
