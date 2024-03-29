#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <asm/ioctl.h>
#include <drm/drm.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

int drm_encoder_init(struct drm_device* dev, struct drm_encoder* encoder,
                     int encoder_type)
{
    struct drm_mode_config* config = &dev->mode_config;
    int retval;

    if (config->num_encoder >= 32) {
        return EINVAL;
    }

    encoder->encoder_type = encoder_type;
    encoder->encoder_type_id = 0;

    retval = drm_mode_object_add(dev, &encoder->base, DRM_MODE_OBJECT_ENCODER);
    if (retval) return retval;

    list_add_tail(&encoder->head, &dev->mode_config.encoder_list);
    encoder->index = dev->mode_config.num_encoder++;

    return 0;
}

int drm_mode_getencoder(struct drm_device* dev, endpoint_t endpoint, void* data)
{
    struct drm_mode_get_encoder* enc_resp = data;
    struct drm_encoder* encoder;
    struct drm_crtc* crtc;

    encoder = drm_encoder_find(dev, enc_resp->encoder_id);
    if (!encoder) {
        return ENOENT;
    }

    crtc = encoder->crtc;
    if (crtc) {
        enc_resp->crtc_id = crtc->base.id;
    } else {
        enc_resp->crtc_id = 0;
    }

    enc_resp->encoder_type = encoder->encoder_type;
    enc_resp->encoder_id = encoder->base.id;
    enc_resp->possible_crtcs = encoder->possible_crtcs;
    enc_resp->possible_clones = encoder->possible_clones;

    return 0;
}
