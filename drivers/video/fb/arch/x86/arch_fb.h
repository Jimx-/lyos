#ifndef _VIDEO_ARCH_FB_H_
#define _VIDEO_ARCH_FB_H_

typedef void (*fb_dev_init_func)(int devind);

void fb_init_bochs(int devind);

#endif
