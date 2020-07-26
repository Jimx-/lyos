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

#include <libdevman/libdevman.h>

#include "libdrmdriver.h"

int drm_crtc_init_with_planes(struct drm_device* dev, struct drm_crtc* crtc,
                              struct drm_plane* primary,
                              struct drm_plane* cursor,
                              const struct drm_crtc_funcs* funcs)
{
    struct drm_mode_config* config = &dev->mode_config;
    int retval;

    if (config->num_crtc >= 32) {
        return EINVAL;
    }

    crtc->funcs = funcs;

    retval = drm_mode_object_add(dev, &crtc->base, DRM_MODE_OBJECT_CRTC);
    if (retval) return retval;

    list_add_tail(&crtc->head, &dev->mode_config.crtc_list);
    crtc->index = dev->mode_config.num_crtc++;

    crtc->primary = primary;
    if (primary && !primary->possible_crtcs) {
        primary->possible_crtcs = 1U << crtc->index;
    }

    crtc->cursor = cursor;
    if (cursor && !cursor->possible_crtcs) {
        cursor->possible_crtcs = 1U << crtc->index;
    }

    return 0;
}

int drm_mode_getcrtc(struct drm_device* dev, endpoint_t endpoint, void* data)
{
    return 0;
}

int drm_mode_setcrtc(struct drm_device* dev, endpoint_t endpoint, void* data)
{
    struct drm_mode_crtc* crtc_req = data;
    struct drm_mode_config* config = &dev->mode_config;
    struct drm_crtc* crtc;
    struct drm_framebuffer* fb = NULL;
    struct drm_connector* connector;
    struct drm_display_mode* mode = NULL;
    struct drm_connector** connectors = NULL;
    struct drm_mode_set set;
    uint32_t* user_conn_ptr;
    uint32_t* conn_ids = NULL;
    int i, retval;

    crtc = drm_crtc_lookup(dev, crtc_req->crtc_id);
    if (!crtc) return ENOENT;

    if (crtc_req->mode_valid) {
        if (crtc_req->fb_id == -1) {
            retval = EINVAL;
            goto out;
        } else {
            fb = drm_framebuffer_lookup(dev, crtc_req->fb_id);
            if (!fb) {
                retval = EINVAL;
                goto out;
            }
        }

        mode = malloc(sizeof(*mode));
        if (!mode) {
            retval = ENOMEM;
            goto out;
        }

        retval = drm_mode_convert_umode(mode, &crtc_req->mode);
        if (retval) goto out;
    }

    if (crtc_req->count_connectors == 0 && mode) {
        retval = EINVAL;
        goto out;
    }

    if (crtc_req->count_connectors > 0 && (!mode || !fb)) {
        retval = EINVAL;
        goto out;
    }

    if (crtc_req->count_connectors > 0) {
        if (crtc_req->count_connectors > config->num_connector) {
            retval = EINVAL;
            goto out;
        }

        connectors = calloc(sizeof(connectors[0]), crtc_req->count_connectors);
        if (!connectors) {
            retval = ENOMEM;
            goto out;
        }

        conn_ids = calloc(sizeof(conn_ids[0]), crtc_req->count_connectors);
        if (!conn_ids) {
            retval = ENOMEM;
            goto out;
        }

        user_conn_ptr = (uint32_t*)(uintptr_t)crtc_req->set_connectors_ptr;
        retval = data_copy(SELF, conn_ids, endpoint, user_conn_ptr,
                           sizeof(conn_ids[0]) * crtc_req->count_connectors);
        if (retval) {
            free(conn_ids);
            goto out;
        }

        for (i = 0; i < crtc_req->count_connectors; i++) {
            connectors[i] = NULL;

            connector = drm_connector_lookup(dev, conn_ids[i]);
            if (!connector) {
                retval = ENOENT;
                free(conn_ids);
                goto out;
            }

            connectors[i] = connector;
        }
    }

    set.crtc = crtc;
    set.x = crtc_req->x;
    set.y = crtc_req->y;
    set.mode = mode;
    set.connectors = connectors;
    set.num_connectors = crtc_req->count_connectors;
    set.fb = fb;

    retval = crtc->funcs->set_config(&set);

out:
    if (connectors) free(connectors);

    if (mode) free(mode);

    return retval;
}
