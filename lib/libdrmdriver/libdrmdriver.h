#ifndef _LIBDRMDRIVER_H_
#define _LIBDRMDRIVER_H_

struct drm_driver {};

int drmdriver_task(device_id_t dev_id, struct drm_driver* drm_driver);

#endif
