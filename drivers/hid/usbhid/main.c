#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <asm/const.h>
#include <uapi/linux/hid.h>
#include <uapi/linux/input-event-codes.h>

#include "libasyncdriver/libasyncdriver.h"

#include "usbhid.h"

#define NAME "usbhid"

#define MAX_THREADS 2

#define unk KEY_UNKNOWN

static const unsigned char hid_keyboard[256] = {
    0,   0,   0,   0,   30,  48,  46,  32,  18,  33,  34,  35,  23,  36,  37,
    38,  50,  49,  24,  25,  16,  19,  31,  20,  22,  47,  17,  45,  21,  44,
    2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  28,  1,   14,  15,  57,
    12,  13,  26,  27,  43,  43,  39,  40,  41,  51,  52,  53,  58,  59,  60,
    61,  62,  63,  64,  65,  66,  67,  68,  87,  88,  99,  70,  119, 110, 102,
    104, 111, 107, 109, 106, 105, 108, 103, 69,  98,  55,  74,  78,  96,  79,
    80,  81,  75,  76,  77,  71,  72,  73,  82,  83,  86,  127, 116, 117, 183,
    184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 134, 138, 130, 132,
    128, 129, 131, 137, 133, 135, 136, 113, 115, 114, unk, unk, unk, 121, unk,
    89,  93,  124, 92,  94,  95,  unk, unk, unk, 122, 123, 90,  91,  85,  unk,
    unk, unk, unk, unk, unk, unk, 111, unk, unk, unk, unk, unk, unk, unk, unk,
    unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk,
    unk, unk, 179, 180, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk,
    unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk, unk,
    unk, unk, unk, unk, unk, unk, 111, unk, unk, unk, unk, unk, unk, unk, 29,
    42,  56,  125, 97,  54,  100, 126, 164, 166, 165, 163, 161, 115, 114, 113,
    150, 158, 159, 128, 136, 177, 178, 176, 142, 152, 173, 140, unk, unk, unk,
    unk};

static struct usbhid_device usbhid_device;

static int usbhid_process_on_thread(const MESSAGE* msg);
static void usbhid_process(MESSAGE* msg);

static const struct asyncdriver asyncdriver = {
    .name = "usbhid_async",

    .process_on_thread = usbhid_process_on_thread,
    .process = usbhid_process,
};

static s32 snto32(u32 value, unsigned n)
{
    if (!value || !n) return 0;

    if (n > 32) n = 32;

    switch (n) {
    case 8:
        return ((s8)value);
    case 16:
        return ((s16)value);
    case 32:
        return ((s32)value);
    }
    return value & (1 << (n - 1)) ? value | (~0U << n) : value;
}

static int usbhid_process_on_thread(const MESSAGE* msg)
{
    return msg->type != NOTIFY_MSG && msg->type != USB_RQ_COMPLETE_URB;
}

static int parse_endpoint_descriptor(struct usbhid_device* usbhid_dev,
                                     struct usb_descriptor_header* desc)
{
    struct usb_endpoint_descriptor* ep_desc =
        (struct usb_endpoint_descriptor*)desc;
    struct urb_ep_config* ep;

    if (!usb_endpoint_xfer_int(ep_desc)) return 0;

    if (usb_endpoint_dir_in(ep_desc)) {
        ep = &usbhid_dev->ep_in;
        ep->direction = USB_IN;
    } else {
        ep = &usbhid_dev->ep_out;
        ep->direction = USB_OUT;
    }

    if (ep->ep_num != URB_INVALID_EP) return EBUSY;

    ep->ep_num = usb_endpoint_num(ep_desc);
    ep->type = USB_TRANSFER_INT;
    ep->max_packet_size = le16_to_cpu(ep_desc->wMaxPacketSize);
    ep->interval = ep_desc->bInterval;
    return 0;
}

static int hid_get_class_descriptor(struct usbhid_device* usbhid_dev,
                                    unsigned char type, u8* buf, int size)
{
    struct usbhid_urb urb;
    struct urb_ep_config ep_config;
    struct usb_ctrlrequest setup_buf;

    ep_config.ep_num = 0;
    ep_config.type = USB_TRANSFER_CTL;
    ep_config.direction = USB_IN;
    ep_config.interval = 0;

    urb_helper_init_urb(&urb, usbhid_dev->dev_id, &ep_config);

    setup_buf.bRequestType = USB_RECIP_INTERFACE | USB_DIR_IN;
    setup_buf.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup_buf.wValue = type << 8;
    setup_buf.wIndex = 0x00;
    setup_buf.wLength = cpu_to_le16(size);
    urb_helper_attach_data(&urb, TRUE, &setup_buf, sizeof(setup_buf));
    urb_helper_attach_data(&urb, FALSE, buf, size);

    return urb_helper_send_wait_urb(&urb);
}

static int parse_hid_descriptor(struct usbhid_device* usbhid_dev,
                                struct usb_descriptor_header* desc)
{
    struct hid_descriptor* hdesc = (struct hid_descriptor*)desc;
    unsigned int rsize = 0;
    int num_descriptors;
    size_t offset = offsetof(struct hid_descriptor, desc);
    u8* rdesc;
    int i;
    int retval;

    num_descriptors =
        min(hdesc->bNumDescriptors,
            (hdesc->bLength - offset) / sizeof(struct hid_class_descriptor));

    for (i = 0; i < num_descriptors; i++)
        if (hdesc->desc[i].bDescriptorType == HID_DT_REPORT)
            rsize = le16_to_cpu(hdesc->desc[i].wDescriptorLength);

    if (!rsize || rsize > HID_MAX_DESCRIPTOR_SIZE) return EINVAL;

    rdesc = malloc(rsize);
    if (!rdesc) return ENOMEM;

    retval = hid_get_class_descriptor(usbhid_dev, HID_DT_REPORT, rdesc, rsize);
    if (retval) {
        free(rdesc);
        return retval;
    }

    retval = hid_parse_descriptor(usbhid_dev, rdesc, rsize);
    free(rdesc);
    return retval;
}

static int parse_descriptors(struct usbhid_device* usbhid_dev, const char* buf,
                             size_t buf_len)
{
    struct usb_descriptor_header* cur_desc;
    struct usb_interface_descriptor* intf_desc;
    size_t cur_byte;
    int valid_interface = FALSE;
    int retval;

    cur_byte = 0;
    while (cur_byte < buf_len) {
        cur_desc = (struct usb_descriptor_header*)&buf[cur_byte];

        if ((cur_desc->bLength > 3) &&
            (cur_byte + cur_desc->bLength <= buf_len)) {

            /* Parse based on descriptor type */
            switch (cur_desc->bDescriptorType) {
            case USB_DT_INTERFACE:
                intf_desc = (struct usb_interface_descriptor*)cur_desc;
                valid_interface = ((1 << intf_desc->bInterfaceNumber) &
                                   usbhid_dev->interfaces) != 0;
                break;

            case USB_DT_ENDPOINT:
                if (!valid_interface) break;

                retval = parse_endpoint_descriptor(usbhid_dev, cur_desc);
                if (retval) return retval;
                break;

            case HID_DT_HID:
                retval = parse_hid_descriptor(usbhid_dev, cur_desc);
                if (retval) return retval;

            default:
                break;
            }
        }

        cur_byte += cur_desc->bLength;
    }

    if (usbhid_dev->ep_in.ep_num == URB_INVALID_EP) return ENODEV;

    return 0;
}

static int usbhid_get_configuration(struct usbhid_device* usbhid_dev)
{
    struct usbhid_urb urb;
    struct urb_ep_config ep_config;
    struct usb_ctrlrequest setup_buf;
    char descriptors[128];
    int retval;

    ep_config.ep_num = 0;
    ep_config.type = USB_TRANSFER_CTL;
    ep_config.direction = USB_IN;
    ep_config.interval = 0;

    urb_helper_init_urb(&urb, usbhid_dev->dev_id, &ep_config);

    setup_buf.bRequestType = USB_DIR_IN;
    setup_buf.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup_buf.wValue = USB_DT_CONFIG << 8;
    setup_buf.wIndex = 0x00;
    setup_buf.wLength = 128;
    urb_helper_attach_data(&urb, TRUE, &setup_buf, sizeof(setup_buf));
    urb_helper_attach_data(&urb, FALSE, descriptors, sizeof(descriptors));

    retval = urb_helper_send_wait_urb(&urb);
    if (retval) return retval;

    retval = parse_descriptors(usbhid_dev, descriptors, urb.actual_length);

    return retval;
}

static struct hid_report* hid_get_report(struct hid_report_enum* report_enum,
                                         const u8* data)
{
    struct hid_report* report;
    unsigned int n = 0;

    if (report_enum->numbered) n = *data;

    report = report_enum->report_id_hash[n];

    return report;
}

static size_t hid_compute_report_size(struct hid_report* report)
{
    if (report->size) return ((report->size - 1) >> 3) + 1;

    return 0;
}

static u32 extract_field(u8* report, unsigned offset, int n)
{
    unsigned int idx = offset / 8;
    unsigned int bit_nr = 0;
    unsigned int bit_shift = offset % 8;
    int bits_to_copy = 8 - bit_shift;
    u32 value = 0;
    u32 mask = n < 32 ? (1U << n) - 1 : ~0U;

    while (n > 0) {
        value |= ((u32)report[idx] >> bit_shift) << bit_nr;
        n -= bits_to_copy;
        bit_nr += bits_to_copy;
        bits_to_copy = 8;
        bit_shift = 0;
        idx++;
    }

    return value & mask;
}

static u32 hid_field_extract(u8* report, unsigned offset, int n)
{
    if (n > 32) n = 32;
    return extract_field(report, offset, n);
}

static void hid_input_fetch_field(struct usbhid_device* dev,
                                  struct hid_field* field, __u8* data)
{
    unsigned n;
    unsigned count = field->report_count;
    unsigned offset = field->report_offset;
    unsigned size = field->report_size;
    __s32 min = field->logical_minimum;
    __s32* value;

    value = field->new_value;
    memset(value, 0, count * sizeof(__s32));
    field->ignored = FALSE;

    for (n = 0; n < count; n++) {
        value[n] =
            min < 0
                ? snto32(hid_field_extract(data, offset + n * size, size), size)
                : hid_field_extract(data, offset + n * size, size);
    }
}

static inline int hid_array_value_is_valid(struct hid_field* field, s32 value)
{
    s32 min = field->logical_minimum;

    return value >= min && value <= field->logical_maximum &&
           value - min < field->maxusage;
}

static void hid_process_event(struct usbhid_device* dev,
                              struct hid_field* field, struct hid_usage* usage,
                              s32 value)
{
    struct inputdriver_dev* input_dev = &dev->input_dev;
    if (!usage->type) return;

    switch (usage->type) {
    case EV_KEY:
        if (usage->code == 0) return;
        break;
    }

    /* inputdriver_send_event(input_dev, EV_MSC, MSC_SCAN, scancode); */

    inputdriver_send_event(input_dev, usage->type, usage->code, value);
}

static int search(s32* array, s32 value, unsigned n)
{
    while (n--) {
        if (*array++ == value) return 0;
    }
    return -1;
}

static void hid_input_array_field(struct usbhid_device* dev,
                                  struct hid_field* field)
{
    unsigned int n;
    unsigned int count = field->report_count;
    s32 min = field->logical_minimum;
    s32* value;

    value = field->new_value;

    if (field->ignored) return;

    for (n = 0; n < count; n++) {
        if (hid_array_value_is_valid(field, field->value[n]) &&
            search(value, field->value[n], count))
            hid_process_event(dev, field, &field->usage[field->value[n] - min],
                              0);

        if (hid_array_value_is_valid(field, value[n]) &&
            search(field->value, value[n], count))
            hid_process_event(dev, field, &field->usage[value[n] - min], 1);
    }

    memcpy(field->value, value, count * sizeof(s32));
}

static void hid_process_report(struct usbhid_device* dev,
                               struct hid_report* report, u8* data)
{
    struct hid_field_entry* entry;
    struct hid_field* field;
    int i;

    for (i = 0; i < report->maxfield; i++)
        hid_input_fetch_field(dev, report->field[i], data);

    if (!list_empty(&report->field_entry_list)) {
        list_for_each_entry(entry, &report->field_entry_list, list)
        {
            field = entry->field;

            if (field->flags & HID_MAIN_ITEM_VARIABLE)
                hid_process_event(dev, field, &field->usage[entry->index],
                                  field->new_value[entry->index]);
            else
                hid_input_array_field(dev, field);
        }

        for (i = 0; i < report->maxfield; i++) {
            field = report->field[i];

            if (field->flags & HID_MAIN_ITEM_VARIABLE)
                memcpy(field->value, field->new_value,
                       field->report_count * sizeof(s32));
        }
    }
}

static int hid_input_report(struct usbhid_device* dev,
                            enum hid_report_type type, u8* data, u32 size)
{
    struct inputdriver_dev* input_dev = &dev->input_dev;
    struct hid_report_enum* report_enum = &dev->report_enum[type];
    struct hid_report* report;
    int max_buffer_size = HID_MAX_BUFFER_SIZE;
    u32 rsize, csize = size;
    u8* cdata = data;

    report = hid_get_report(report_enum, data);
    if (!report) return 0;

    if (report_enum->numbered) {
        cdata++;
        csize--;
    }

    rsize = hid_compute_report_size(report);

    if (report_enum->numbered && rsize >= max_buffer_size)
        rsize = max_buffer_size - 1;
    else if (rsize > max_buffer_size)
        rsize = max_buffer_size;

    if (csize < rsize) memset(cdata + csize, 0, rsize - csize);

    if (report->maxfield) {
        hid_process_report(dev, report, cdata);
    }

    inputdriver_sync(input_dev);

    return 0;
}

static void usbhid_device_run(struct usbhid_device* dev)
{
    struct usbhid_urb urb;
    struct urb_ep_config ep_config;
    u8 descriptors[128];
    int retval;

    dev->running = TRUE;

    ep_config.ep_num = dev->ep_in.ep_num;
    ep_config.type = USB_TRANSFER_INT;
    ep_config.direction = USB_IN;
    ep_config.interval = dev->ep_in.interval;

    urb_helper_init_urb(&urb, dev->dev_id, &ep_config);

    urb_helper_attach_data(&urb, FALSE, descriptors, sizeof(descriptors));

    while (dev->running) {
        retval = urb_helper_send_wait_urb(&urb);
        if (retval) return;

        hid_input_report(dev, HID_INPUT_REPORT, descriptors, urb.actual_length);
    }
}

static void insert_field_entry(struct usbhid_device* hid,
                               struct hid_report* report,
                               struct hid_field_entry* entry,
                               struct hid_field* field,
                               unsigned int usage_index)
{
    struct hid_field_entry* next;

    entry->field = field;
    entry->index = usage_index;
    entry->priority = field->usages_priorities[usage_index];

    list_for_each_entry(next, &report->field_entry_list, list)
    {
        if (entry->priority > next->priority) {
            list_add_tail(&entry->list, &next->list);
            return;
        }
    }

    list_add_tail(&entry->list, &report->field_entry_list);
}

static void usbhid_report_process_ordering(struct usbhid_device* dev,
                                           struct hid_report* report)
{
    struct hid_field* field;
    struct hid_field_entry* entries;
    unsigned int count = 0;
    unsigned int usages;
    int i, j;

    for (i = 0; i < report->maxfield; i++) {
        field = report->field[i];

        if (field->flags & HID_MAIN_ITEM_VARIABLE)
            count += field->report_count;
        else
            count++;
    }

    entries = calloc(count, sizeof(*entries));
    if (!entries) return;

    report->field_entries = entries;

    usages = 0;
    for (i = 0; i < report->maxfield; i++) {
        field = report->field[i];

        if (field->flags & HID_MAIN_ITEM_VARIABLE) {
            for (j = 0; j < field->report_count; j++) {
                insert_field_entry(dev, report, &entries[usages], field, j);
                usages++;
            }
        } else {
            insert_field_entry(dev, report, &entries[usages], field, 0);
            usages++;
        }
    }
}

static inline void usbhid_map_usage(struct usbhid_device* dev,
                                    struct hid_usage* usage, bitchunk_t** bit,
                                    int* max, u8 type, unsigned int c)
{
    struct inputdriver_dev* input_dev = &dev->input_dev;
    unsigned int limit = 0;
    bitchunk_t* bmap = NULL;

    switch (type) {
    case EV_ABS:
        bmap = input_dev->absbit;
        limit = ABS_MAX;
        break;
    case EV_REL:
        bmap = input_dev->relbit;
        limit = REL_MAX;
        break;
    case EV_KEY:
        bmap = input_dev->keybit;
        limit = KEY_MAX;
        break;
    case EV_LED:
        bmap = input_dev->ledbit;
        limit = LED_MAX;
        break;
    case EV_MSC:
        bmap = input_dev->mscbit;
        limit = MSC_MAX;
        break;
    }

    if (c > limit) {
        return;
    }

    usage->type = type;
    usage->code = c;
    *max = limit;
    *bit = bmap;
}

#define map_key(c) usbhid_map_usage(dev, usage, &bit, &max, EV_KEY, (c))

static void hid_configure_usage(struct usbhid_device* dev,
                                struct hid_field* field,
                                struct hid_usage* usage,
                                unsigned int usage_index)
{
    struct inputdriver_dev* input_dev = &dev->input_dev;
    int max = 0;
    bitchunk_t* bit = NULL;

    if (field->report_count < 1) goto ignore;

    switch (usage->hid & HID_USAGE_PAGE) {
    case HID_UP_KEYBOARD:
        if ((usage->hid & HID_USAGE) < 256) {
            if (!hid_keyboard[usage->hid & HID_USAGE]) goto ignore;
            map_key(hid_keyboard[usage->hid & HID_USAGE]);
        } else
            map_key(KEY_UNKNOWN);

        break;
    }

    if (!bit) return;

    SET_BIT(input_dev->evbit, usage->type);

    if (usage->code <= max)
        SET_BIT(bit, usage->code);
    else
        goto ignore;

    if (usage->type == EV_KEY) {
        SET_BIT(input_dev->evbit, EV_MSC);
        SET_BIT(input_dev->mscbit, MSC_SCAN);
    }

    return;

ignore:
    usage->type = 0;
    usage->code = 0;
}

static void hid_configure_usages(struct usbhid_device* dev,
                                 struct hid_report* report)
{
    int i, j;

    for (i = 0; i < report->maxfield; i++)
        for (j = 0; j < report->field[i]->maxusage; j++)
            hid_configure_usage(dev, report->field[i],
                                report->field[i]->usage + j, j);
}

static void usbhid_process_ordering(struct usbhid_device* dev)
{
    struct hid_report* report;
    struct hid_report_enum* report_enum = &dev->report_enum[HID_INPUT_REPORT];

    list_for_each_entry(report, &report_enum->report_list, list)
        usbhid_report_process_ordering(dev, report);
}

static void usbhid_device_connect(device_id_t dev_id, unsigned int interfaces)
{
    struct usbhid_device* dev = &usbhid_device;
    struct inputdriver_dev* input_dev = &dev->input_dev;
    struct hid_report* report;
    int i;
    int retval;

    /* Already connected */
    if (dev->dev_id != NO_DEVICE_ID) return;

    dev->dev_id = dev_id;
    dev->interfaces = interfaces;

    inputdriver_device_init(input_dev, NULL, dev_id);

    input_dev->input_id.bustype = BUS_USB;
    input_dev->input_id.vendor = 0x0001;
    input_dev->input_id.product = 1;
    input_dev->input_id.version = 1;

    retval = usbhid_get_configuration(dev);
    if (retval) return;

    usbhid_process_ordering(dev);

    for (i = HID_INPUT_REPORT; i <= HID_OUTPUT_REPORT; i++) {
        list_for_each_entry(report, &dev->report_enum[i].report_list, list)
        {
            if (!report->maxfield) continue;

            hid_configure_usages(dev, report);
        }
    }

    inputdriver_register_device(input_dev);

    usbhid_device_run(dev);
}

static const struct libusb_driver usb_driver = {
    .connect_device = usbhid_device_connect,
    .complete_urb = urb_helper_complete_urb,
};

static void usbhid_process(MESSAGE* msg)
{
    usb_handle_message(&usb_driver, msg);
}

static void usbhid_device_init(struct usbhid_device* dev)
{
    int i;

    dev->dev_id = NO_DEVICE_ID;
    dev->ep_in.ep_num = URB_INVALID_EP;
    dev->ep_out.ep_num = URB_INVALID_EP;

    for (i = 0; i < HID_REPORT_TYPES; i++) {
        INIT_LIST_HEAD(&dev->report_enum[i].report_list);
    }
}

static int usbhid_init(void)
{
    printl(NAME ": USB HID driver is running\n");

    usbhid_device_init(&usbhid_device);

    usb_init();

    return 0;
}

int main()
{
    serv_register_init_fresh_callback(usbhid_init);
    serv_init();

    asyncdrv_task(&asyncdriver, MAX_THREADS, NULL);

    return 0;
}
