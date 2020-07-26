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

void drm_mode_config_init(struct drm_device* dev)
{
    struct drm_mode_config* config = &dev->mode_config;
    INIT_LIST_HEAD(&config->fb_list);
    INIT_LIST_HEAD(&config->crtc_list);
    INIT_LIST_HEAD(&config->connector_list);
    INIT_LIST_HEAD(&config->encoder_list);
    INIT_LIST_HEAD(&config->plane_list);
    idr_init(&config->object_idr);

    config->num_fb = 0;
    config->num_crtc = 0;
    config->num_encoder = 0;
    config->num_connector = 0;
    config->num_total_plane = 0;
}

void drm_mode_config_reset(struct drm_device* dev)
{
    struct drm_crtc* crtc;
    struct drm_plane* plane;

    drm_for_each_plane(plane, dev)
    {
        if (plane->state) {
            free(plane->state);
        }

        plane->state = malloc(sizeof(*plane->state));
        memset(plane->state, 0, sizeof(*plane->state));
    }

    drm_for_each_crtc(crtc, dev)
    {
        if (crtc->state) {
            free(crtc->state);
        }

        crtc->state = malloc(sizeof(*crtc->state));
        memset(crtc->state, 0, sizeof(*crtc->state));

        crtc->state->crtc = crtc;
    }
}

int drm_mode_getresources(struct drm_device* dev, endpoint_t endpoint,
                          void* data)
{
    struct drm_mode_card_res* card_res = data;
    struct drm_mode_config* config = &dev->mode_config;
    struct drm_crtc* crtc;
    struct drm_encoder* encoder;
    struct drm_connector* connector;
#define ID_BUF_SIZE 32
    uint32_t id_buf[ID_BUF_SIZE], *crtc_id, *encoder_id, *connector_id;
    int count, copy_count;
    int retval;

    count = copy_count = 0;
    card_res->count_fbs = count;

    card_res->max_width = config->max_width;
    card_res->max_height = config->max_height;
    card_res->min_width = config->min_width;
    card_res->max_width = config->max_width;

    count = copy_count = 0;
    crtc_id = (void*)((uintptr_t)card_res->crtc_id_ptr);
    drm_for_each_crtc(crtc, dev)
    {
        if (count < card_res->count_crtcs) {
            id_buf[copy_count++] = crtc->base.id;

            if (copy_count == ID_BUF_SIZE) {
                retval = data_copy(endpoint, crtc_id, SELF, id_buf,
                                   copy_count * sizeof(id_buf[0]));
                if (retval) return retval;

                crtc_id += copy_count;
                copy_count = 0;
            }
        }

        count++;
    }

    if (copy_count) {
        retval = data_copy(endpoint, crtc_id, SELF, id_buf,
                           copy_count * sizeof(id_buf[0]));
        if (retval) return retval;
    }
    card_res->count_crtcs = count;

    count = copy_count = 0;
    encoder_id = (void*)((uintptr_t)card_res->encoder_id_ptr);
    drm_for_each_encoder(encoder, dev)
    {
        if (count < card_res->count_encoders) {
            id_buf[copy_count++] = encoder->base.id;

            if (copy_count == ID_BUF_SIZE) {
                retval = data_copy(endpoint, encoder_id, SELF, id_buf,
                                   copy_count * sizeof(id_buf[0]));
                if (retval) return retval;

                encoder_id += copy_count;
                copy_count = 0;
            }
        }

        count++;
    }

    if (copy_count) {
        retval = data_copy(endpoint, encoder_id, SELF, id_buf,
                           copy_count * sizeof(id_buf[0]));
        if (retval) return retval;
    }
    card_res->count_encoders = count;

    count = copy_count = 0;
    connector_id = (void*)((uintptr_t)card_res->connector_id_ptr);
    drm_for_each_connector(connector, dev)
    {
        if (count < card_res->count_connectors) {
            id_buf[copy_count++] = connector->base.id;

            if (copy_count == ID_BUF_SIZE) {
                retval = data_copy(endpoint, connector_id, SELF, id_buf,
                                   copy_count * sizeof(id_buf[0]));
                if (retval) return retval;

                connector_id += copy_count;
            }
        }

        count++;
    }

    if (copy_count) {
        retval = data_copy(endpoint, connector_id, SELF, id_buf,
                           copy_count * sizeof(id_buf[0]));
        if (retval) return retval;
    }
    card_res->count_connectors = count;

    return 0;
}
