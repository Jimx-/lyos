#ifndef _LIBDRMDRIVER_PROTO_H_
#define _LIBDRMDRIVER_PROTO_H_

#include <libchardriver/libchardriver.h>

int drm_do_ioctl(struct drm_device* dev, int request, endpoint_t endpoint,
                 mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id);

/* connector.c */
int drm_mode_getconnector(struct drm_device* dev, endpoint_t enpoint,
                          void* data);

/* crtc.c */
int drm_mode_getcrtc(struct drm_device* dev, endpoint_t endpoint, void* data);
int drm_mode_setcrtc(struct drm_device* dev, endpoint_t endpoint, void* data);
int drm_atomic_page_flip(struct drm_crtc* crtc, struct drm_framebuffer* fb);

/* dumb_buffer.c */
int drm_mode_create_dumb_ioctl(struct drm_device* dev, endpoint_t endpoint,
                               void* data);
int drm_mode_mmap_dumb_ioctl(struct drm_device* dev, endpoint_t endpoint,
                             void* data);

/* encoder.c */
int drm_mode_getencoder(struct drm_device* dev, endpoint_t endpoint,
                        void* data);

/* framebuffer.c */
int drm_mode_addfb_ioctl(struct drm_device* dev, endpoint_t endpoint,
                         void* data);

/* mode_config.c */
int drm_mode_getresources(struct drm_device* dev, endpoint_t endpoint,
                          void* data);

/* gem.c */
void drm_gem_open(struct drm_device* dev);
int drm_gem_mmap(struct drm_device* dev, endpoint_t endpoint, char* addr,
                 off_t offset, size_t length, char** retaddr);

/* plane.c */
int drm_mode_page_flip_ioctl(struct drm_device* dev, endpoint_t endpoint,
                             void* data);

#endif
