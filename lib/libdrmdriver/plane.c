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

int drm_plane_init(struct drm_device* dev, struct drm_plane* plane,
                   uint32_t possible_crtcs, enum drm_plane_type type)
{
    int retval;

    retval = drm_mode_object_add(dev, &plane->base, DRM_MODE_OBJECT_PLANE);
    if (retval) return retval;

    plane->possible_crtcs = possible_crtcs;
    plane->type = type;

    list_add_tail(&plane->head, &dev->mode_config.plane_list);
    plane->index = dev->mode_config.num_total_plane++;

    return 0;
}

void drm_helper_commit_planes(struct drm_device* dev,
                              struct drm_atomic_state* old_state)
{
    struct drm_plane* plane;
    int i;

    for (i = 0; i < dev->mode_config.num_total_plane; i++) {
        struct drm_plane_helper_funcs* funcs;

        plane = old_state->planes[i].ptr;
        funcs = plane->helper_funcs;

        funcs->update(plane, old_state->planes[i].old_state);
    }
}
