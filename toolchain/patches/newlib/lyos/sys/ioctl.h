#ifndef _IOCTL_H_
#define _IOCTL_H_

#include <asm/ioctls.h>

#define TCGETS 0x4000
#define TCSETS 0x4001
#define TCSETSW 0x4002
#define TCSETSF 0x4003
#define TIOCGPGRP 0x4004
#define TIOCSPGRP 0x4005
#define TCFLSH 0x4006
#define TIOCGWINSZ 0x4007
#define TIOCSWINSZ 0x4008

#ifdef __cplusplus
extern "C"
{
#endif

    int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif /* _IOCTL_H_ */
