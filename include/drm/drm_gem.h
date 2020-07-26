#ifndef _DRM_DRM_GEM_H_
#define _DRM_DRM_GEM_H_

struct drm_gem_object;
struct drm_gem_object_funcs {
    int (*mmap)(struct drm_gem_object* obj, endpoint_t endpoint, char* addr,
                size_t length, char** retaddr);
};

struct drm_gem_object {
    size_t size;

    const struct drm_gem_object_funcs* funcs;
};

int drm_gem_object_init(struct drm_gem_object* obj, size_t size);
struct drm_gem_object* drm_gem_object_lookup(struct drm_device* dev,
                                             u32 handle);
int drm_gem_handle_create(struct drm_device* dev, struct drm_gem_object* obj,
                          unsigned int* handlep);
int drm_gem_dumb_map_offset(struct drm_device* dev, u32 handle, u64* offset);

#endif
