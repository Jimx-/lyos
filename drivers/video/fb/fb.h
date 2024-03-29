#ifndef _VIDEO_FB_H_
#define _VIDEO_FB_H_

#include <libdevman/libdevman.h>

struct fb_videomode {
    u32 xres;
    u32 yres;
};

struct fb_info {
    device_id_t dev_id;
    struct fb_videomode* mode;

    void* screen_base;
    phys_bytes screen_base_phys;
    size_t screen_size;
};

int register_framebuffer(struct fb_info* fb);

void arch_init_fb(void);

#endif
