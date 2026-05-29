#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/sysutils.h>
#include <uapi/linux/hid.h>

#include "usbhid.h"

#define HID_GLOBAL_STACK_SIZE     4
#define HID_COLLECTION_STACK_SIZE 4

struct hid_parser {
    struct hid_global global;
    struct hid_global global_stack[HID_GLOBAL_STACK_SIZE];
    unsigned int global_stack_ptr;
    struct hid_local local;
    unsigned int* collection_stack;
    unsigned int collection_stack_ptr;
    unsigned int collection_stack_size;
    struct usbhid_device* device;
    unsigned int scan_flags;
};

static u32 item_udata(struct hid_item* item)
{
    switch (item->size) {
    case 1:
        return item->data.u8;
    case 2:
        return item->data.u16;
    case 4:
        return item->data.u32;
    }
    return 0;
}

static s32 item_sdata(struct hid_item* item)
{
    switch (item->size) {
    case 1:
        return item->data.s8;
    case 2:
        return item->data.s16;
    case 4:
        return item->data.s32;
    }
    return 0;
}

static const u8* fetch_item(const u8* start, const u8* end,
                            struct hid_item* item)
{
    u8 b;

    if ((end - start) <= 0) return NULL;

    b = *start++;

    item->type = (b >> 2) & 3;
    item->tag = (b >> 4) & 15;

    if (item->tag == HID_ITEM_TAG_LONG) {

        item->format = HID_ITEM_FORMAT_LONG;

        if ((end - start) < 2) return NULL;

        item->size = *start++;
        item->tag = *start++;

        if ((end - start) < item->size) return NULL;

        item->data.longdata = start;
        start += item->size;
        return start;
    }

    item->format = HID_ITEM_FORMAT_SHORT;
    item->size = b & 3;

    switch (item->size) {
    case 0:
        return start;

    case 1:
        if ((end - start) < 1) return NULL;
        item->data.u8 = *start++;
        return start;

    case 2:
        if ((end - start) < 2) return NULL;
        item->data.u16 = le16_to_cpu(*(__le16*)start);
        start = (u8*)((__le16*)start + 1);
        return start;

    case 3:
        item->size++;
        if ((end - start) < 4) return NULL;
        item->data.u32 = le32_to_cpu(*(__le32*)start);
        start = (u8*)((__le32*)start + 1);
        return start;
    }

    return NULL;
}

static int open_collection(struct hid_parser* parser, unsigned type)
{
    struct hid_collection* collection;
    unsigned usage;
    int collection_index;

    usage = parser->local.usage[0];

    if (parser->collection_stack_ptr == parser->collection_stack_size) {
        unsigned int* collection_stack;
        unsigned int new_size =
            parser->collection_stack_size + HID_COLLECTION_STACK_SIZE;

        collection_stack =
            realloc(parser->collection_stack, new_size * sizeof(unsigned int));
        if (!collection_stack) return -ENOMEM;

        parser->collection_stack = collection_stack;
        parser->collection_stack_size = new_size;
    }

    if (parser->device->maxcollection == parser->device->collection_size) {
        collection = malloc(parser->device->collection_size * 2 *
                            sizeof(struct hid_collection));
        if (collection == NULL) {
            return ENOMEM;
        }
        memcpy(collection, parser->device->collection,
               sizeof(struct hid_collection) * parser->device->collection_size);
        memset(collection + parser->device->collection_size, 0,
               sizeof(struct hid_collection) * parser->device->collection_size);
        free(parser->device->collection);
        parser->device->collection = collection;
        parser->device->collection_size *= 2;
    }

    parser->collection_stack[parser->collection_stack_ptr++] =
        parser->device->maxcollection;

    collection_index = parser->device->maxcollection++;
    collection = parser->device->collection + collection_index;
    collection->type = type;
    collection->usage = usage;
    collection->level = parser->collection_stack_ptr - 1;
    collection->parent_idx =
        (collection->level == 0)
            ? -1
            : parser->collection_stack[collection->level - 1];

    if (type == HID_COLLECTION_APPLICATION) parser->device->maxapplication++;

    return 0;
}

static int close_collection(struct hid_parser* parser)
{
    if (!parser->collection_stack_ptr) return -1;
    parser->collection_stack_ptr--;
    return 0;
}

static unsigned hid_lookup_collection(struct hid_parser* parser, unsigned type)
{
    struct hid_collection* collection = parser->device->collection;
    int n;

    for (n = parser->collection_stack_ptr - 1; n >= 0; n--) {
        unsigned index = parser->collection_stack[n];
        if (collection[index].type == type) return collection[index].usage;
    }
    return 0;
}

static struct hid_report* hid_register_report(struct usbhid_device* device,
                                              enum hid_report_type type,
                                              unsigned int id,
                                              unsigned int application)
{
    struct hid_report_enum* report_enum = &device->report_enum[type];
    struct hid_report* report;

    if (id >= HID_MAX_IDS) return NULL;
    if (report_enum->report_id_hash[id]) return report_enum->report_id_hash[id];

    report = malloc(sizeof(*report));
    if (!report) return NULL;

    memset(report, 0, sizeof(*report));

    if (id != 0) report_enum->numbered = 1;

    report->id = id;
    report->type = type;
    report->size = 0;
    report->device = device;
    report->application = application;
    report_enum->report_id_hash[id] = report;

    list_add_tail(&report->list, &report_enum->report_list);
    INIT_LIST_HEAD(&report->field_entry_list);

    return report;
}

static struct hid_field* hid_register_field(struct hid_report* report,
                                            unsigned usages)
{
    struct hid_field* field;
    size_t alloc_size;

    if (report->maxfield == HID_MAX_FIELDS) return NULL;

    alloc_size = sizeof(struct hid_field) + usages * sizeof(struct hid_usage) +
                 3 * usages * sizeof(unsigned int);
    field = malloc(alloc_size);
    if (!field) return NULL;

    memset(field, 0, alloc_size);

    field->index = report->maxfield++;
    report->field[field->index] = field;
    field->usage = (struct hid_usage*)(field + 1);
    field->value = (s32*)(field->usage + usages);
    field->new_value = (s32*)(field->value + usages);
    field->usages_priorities = (s32*)(field->new_value + usages);
    field->report = report;

    return field;
}

static int hid_add_field(struct hid_parser* parser, unsigned report_type,
                         unsigned flags)
{
    struct hid_report* report;
    struct hid_field* field;
    unsigned int application;
    unsigned int offset;
    unsigned int usages;
    unsigned int i;

    application = hid_lookup_collection(parser, HID_COLLECTION_APPLICATION);

    report = hid_register_report(parser->device, report_type,
                                 parser->global.report_id, application);

    offset = report->size;
    report->size += parser->global.report_size * parser->global.report_count;

    if (!parser->local.usage_index) return 0;

    usages = max(parser->local.usage_index, parser->global.report_count);

    field = hid_register_field(report, usages);
    if (!field) return 0;

    field->physical = hid_lookup_collection(parser, HID_COLLECTION_PHYSICAL);
    field->logical = hid_lookup_collection(parser, HID_COLLECTION_LOGICAL);
    field->application = application;

    for (i = 0; i < usages; i++) {
        unsigned j = i;
        if (i >= parser->local.usage_index) j = parser->local.usage_index - 1;
        field->usage[i].hid = parser->local.usage[j];
        field->usage[i].collection_index = parser->local.collection_index[j];
        field->usage[i].usage_index = i;
        field->usage[i].resolution_multiplier = 1;
    }

    field->maxusage = usages;
    field->flags = flags;
    field->report_offset = offset;
    field->report_type = report_type;
    field->report_size = parser->global.report_size;
    field->report_count = parser->global.report_count;
    field->logical_minimum = parser->global.logical_minimum;
    field->logical_maximum = parser->global.logical_maximum;
    field->physical_minimum = parser->global.physical_minimum;
    field->physical_maximum = parser->global.physical_maximum;
    field->unit_exponent = parser->global.unit_exponent;
    field->unit = parser->global.unit;

    return 0;
}

static int hid_parser_main(struct hid_parser* parser, struct hid_item* item)
{
    u32 data;
    int retval;

    data = item_udata(item);

    switch (item->tag) {
    case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
        retval = open_collection(parser, data & 0xff);
        break;

    case HID_MAIN_ITEM_TAG_END_COLLECTION:
        retval = close_collection(parser);
        break;

    case HID_MAIN_ITEM_TAG_INPUT:
        retval = hid_add_field(parser, HID_INPUT_REPORT, data);
        break;

    case HID_MAIN_ITEM_TAG_OUTPUT:
        retval = hid_add_field(parser, HID_OUTPUT_REPORT, data);
        break;

    case HID_MAIN_ITEM_TAG_FEATURE:
        retval = hid_add_field(parser, HID_FEATURE_REPORT, data);
        break;

    default:
        retval = -1;
        break;
    }

    memset(&parser->local, 0, sizeof(parser->local));
    return retval;
}

static int hid_parser_global(struct hid_parser* parser, struct hid_item* item)
{
    switch (item->tag) {
    case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
        parser->global.usage_page = item_udata(item);
        return 0;

    case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
        parser->global.logical_minimum = item_sdata(item);
        return 0;

    case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
        if (parser->global.logical_minimum < 0)
            parser->global.logical_maximum = item_sdata(item);
        else
            parser->global.logical_maximum = item_udata(item);

    case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
        parser->global.physical_minimum = item_sdata(item);
        return 0;

    case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
        if (parser->global.physical_minimum < 0)
            parser->global.physical_maximum = item_sdata(item);
        else
            parser->global.physical_maximum = item_udata(item);
        return 0;

    case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
        parser->global.report_size = item_udata(item);
        if (parser->global.report_size > 256) return -1;
        return 0;

    case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
        parser->global.report_count = item_udata(item);
        if (parser->global.report_count > HID_MAX_USAGES) return -1;
        return 0;

    default:
        return -1;
    }
}

static void complete_usage(struct hid_parser* parser, unsigned int index)
{
    parser->local.usage[index] &= 0xFFFF;
    parser->local.usage[index] |= (parser->global.usage_page & 0xFFFF) << 16;
}

static int hid_add_usage(struct hid_parser* parser, unsigned usage, u8 size)
{
    if (parser->local.usage_index >= HID_MAX_USAGES) return -1;
    parser->local.usage[parser->local.usage_index] = usage;

    if (size <= 2) complete_usage(parser, parser->local.usage_index);

    parser->local.usage_size[parser->local.usage_index] = size;
    parser->local.collection_index[parser->local.usage_index] =
        parser->collection_stack_ptr
            ? parser->collection_stack[parser->collection_stack_ptr - 1]
            : 0;
    parser->local.usage_index++;
    return 0;
}

static int hid_parser_local(struct hid_parser* parser, struct hid_item* item)
{
    u32 data;
    unsigned int count;
    unsigned int i;

    data = item_udata(item);

    switch (item->tag) {
    case HID_LOCAL_ITEM_TAG_USAGE:
        if (parser->local.delimiter_branch > 1) return 0;

        return hid_add_usage(parser, data, item->size);

    case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:
        if (parser->local.delimiter_branch > 1) return 0;

        parser->local.usage_minimum = data;
        return 0;

    case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:
        if (parser->local.delimiter_branch > 1) return 0;

        count = data - parser->local.usage_minimum;
        if (count + parser->local.usage_index >= HID_MAX_USAGES) {
            data = HID_MAX_USAGES - parser->local.usage_index +
                   parser->local.usage_minimum - 1;
            if (data <= 0) return -1;
        }

        for (i = parser->local.usage_minimum; i <= data; i++) {
            if (hid_add_usage(parser, i, item->size)) return -1;
        }

        return 0;

    default:
        return -1;
    }
}

static int hid_parser_reserved(struct hid_parser* parser, struct hid_item* item)
{
    return 0;
}

int hid_parse_descriptor(struct usbhid_device* usbhid_dev, const u8* buf,
                         int size)
{
    struct hid_item item;
    struct hid_parser* parser;
    const u8* start = buf;
    const u8* end = start + size;
    const u8* next;
    int retval;
    int i;
    static int (*dispatch_type[])(struct hid_parser* parser,
                                  struct hid_item* item) = {
        hid_parser_main, hid_parser_global, hid_parser_local,
        hid_parser_reserved};

    parser = (struct hid_parser*)malloc(sizeof(*parser));
    if (!parser) return ENOMEM;

    memset(parser, 0, sizeof(*parser));
    parser->device = usbhid_dev;

    usbhid_dev->collection =
        calloc(HID_DEFAULT_NUM_COLLECTIONS, sizeof(struct hid_collection));
    if (!usbhid_dev->collection) {
        retval = ENOMEM;
        goto err;
    }

    usbhid_dev->collection_size = HID_DEFAULT_NUM_COLLECTIONS;
    for (i = 0; i < HID_DEFAULT_NUM_COLLECTIONS; i++)
        usbhid_dev->collection[i].parent_idx = -1;

    retval = EINVAL;
    while ((next = fetch_item(start, end, &item)) != NULL) {
        start = next;

        if (item.format != HID_ITEM_FORMAT_SHORT) goto err;

        if (dispatch_type[item.type](parser, &item)) goto err;

        if (start == end) {
            if (parser->collection_stack_ptr) goto err;
            if (parser->local.delimiter_depth) goto err;

            free(parser->collection_stack);
            free(parser);
            return 0;
        }
    }

err:
    free(parser->collection_stack);
    free(parser);
    return retval;
}
