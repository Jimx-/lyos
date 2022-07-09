#ifndef _FBDRM_H_
#define _FBDRM_H_

struct fb_videomode {
    u32 xres;
    u32 yres;
};

struct fb_info {
    struct fb_videomode* mode;

    void* screen_base;
    phys_bytes screen_base_phys;
    size_t screen_size;
};

int register_framebuffer(struct fb_info* fb);

void arch_init_fb(void);

#endif
