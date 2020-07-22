#ifndef _LIBDRMDRIVER_PROTO_H_
#define _LIBDRMDRIVER_PROTO_H_

int drm_do_ioctl(int request, endpoint_t endpoint, char* buf, cdev_id_t id);

/* connector.c */
int drm_mode_getconnector(endpoint_t enpoint, void* data);

/* crtc.c */
int drm_mode_getcrtc(endpoint_t endpoint, void* data);
int drm_mode_setcrtc(endpoint_t endpoint, void* data);

/* dumb_buffer.c */
int drm_mode_create_dumb_ioctl(endpoint_t endpoint, void* data);
int drm_mode_mmap_dumb_ioctl(endpoint_t endpoint, void* data);

/* encoder.c */
int drm_mode_getencoder(endpoint_t endpoint, void* data);

/* framebuffer.c */
int drm_mode_addfb_ioctl(endpoint_t endpoint, void* data);

/* mode_config.c */
int drm_mode_getresources(endpoint_t endpoint, void* data);

/* gem.c */
void drm_gem_open(void);
int drm_gem_mmap(endpoint_t endpoint, char* addr, off_t offset, size_t length,
                 char** retaddr);

#endif
