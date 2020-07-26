#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <errno.h>
#include <asm/ioctl.h>
#include <drm/drm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <libchardriver/libchardriver.h>
#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

int drm_connector_init(struct drm_device* dev, struct drm_connector* connector,
                       int connector_type)
{
    int retval;

    connector->possible_encoders = 0;

    connector->connector_type = connector_type;
    connector->connector_type_id = 0;

    retval =
        drm_mode_object_add(dev, &connector->base, DRM_MODE_OBJECT_CONNECTOR);
    if (retval) return retval;

    INIT_LIST_HEAD(&connector->modes);

    list_add_tail(&connector->head, &dev->mode_config.connector_list);
    connector->index = dev->mode_config.num_connector++;

    return 0;
}

int drm_connector_attach_encoder(struct drm_connector* connector,
                                 struct drm_encoder* encoder)
{
    connector->possible_encoders |= 1U << encoder->index;

    return 0;
}

int drm_mode_getconnector(struct drm_device* dev, endpoint_t endpoint,
                          void* data)
{
    struct drm_mode_get_connector* out_resp = data;
    struct drm_mode_config* config = &dev->mode_config;
    struct drm_connector* connector;
    struct drm_encoder* encoder;
    struct drm_display_mode* mode;
#define MODE_BUF_SIZE 32
    struct drm_mode_modeinfo modes[MODE_BUF_SIZE], *mode_ptr;
    uint32_t* encoder_ptr;
    int encoders_count, modes_count, copy_count;
    uint32_t encoder_ids[32];
    int i, retval;

    connector = drm_connector_lookup(dev, out_resp->connector_id);
    if (!connector) {
        return ENOENT;
    }

    encoders_count = 0;
    for (i = 0; i < 32; i++) {
        if (connector->possible_encoders & (1U << i)) {
            encoders_count++;
        }
    }

    copy_count = 0;
    if ((out_resp->count_encoders >= encoders_count) && encoders_count) {
        drm_for_each_encoder(encoder, dev)
        {
            if (connector->possible_encoders & (1U << encoder->index)) {
                encoder_ids[copy_count++] = encoder->base.id;
            }
        }
    }

    if (copy_count) {
        encoder_ptr = (uint32_t*)(uintptr_t)out_resp->encoders_ptr;

        retval = data_copy(endpoint, encoder_ptr, SELF, encoder_ids,
                           copy_count * sizeof(encoder_ids[0]));
        if (retval) return retval;
    }
    out_resp->count_encoders = encoders_count;

    out_resp->connector_id = connector->base.id;
    out_resp->connector_type = connector->connector_type;
    out_resp->connector_type_id = connector->connector_type_id;

    if (out_resp->count_modes == 0) {
        drm_add_dmt_modes(connector, config->max_width, config->max_height);
    }

    /* out_resp->mm_width = connector->display_info.width_mm; */
    /* out_resp->mm_height = connector->display_info.height_mm; */
    /* out_resp->subpixel = connector->display_info.subpixel_order; */
    out_resp->connection = 1;

    modes_count = 0;
    list_for_each_entry(mode, &connector->modes, head) { modes_count++; }

    if ((out_resp->count_modes >= modes_count) && modes_count) {
        mode_ptr = (struct drm_mode_modeinfo*)(uintptr_t)out_resp->modes_ptr;
        copy_count = 0;

        list_for_each_entry(mode, &connector->modes, head)
        {
            drm_mode_convert_to_umode(&modes[copy_count++], mode);

            if (copy_count == MODE_BUF_SIZE) {
                retval = data_copy(endpoint, mode_ptr, SELF, modes,
                                   copy_count * sizeof(modes[0]));
                if (retval) return retval;

                mode_ptr += copy_count;
                copy_count = 0;
            }
        }

        if (copy_count) {
            retval = data_copy(endpoint, mode_ptr, SELF, modes,
                               copy_count * sizeof(modes[0]));
            if (retval) return retval;
        }
    }
    out_resp->count_modes = modes_count;

    if (connector->encoder) {
        out_resp->encoder_id = connector->encoder->base.id;
    } else {
        out_resp->encoder_id = 0;
    }

    return 0;
}
