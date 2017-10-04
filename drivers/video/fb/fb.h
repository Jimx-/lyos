#ifndef _VIDEO_FB_H_
#define _VIDEO_FB_H_

#define NR_FB_DEVS	1

PUBLIC int arch_init_fb(int minor);
PUBLIC int arch_get_device(int minor, vir_bytes* base, vir_bytes* size);

#endif
