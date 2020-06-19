#ifndef _VIDEO_FB_H_
#define _VIDEO_FB_H_

#define NR_FB_DEVS 1

int arch_init_fb(int minor);
int arch_get_device(int minor, void** base, size_t* size);
int arch_get_device_phys(int minor, phys_bytes* phys_base, phys_bytes* size);

#endif
