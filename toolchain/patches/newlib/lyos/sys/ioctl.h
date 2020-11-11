#ifndef _SYS_IOCTL_H_
#define _SYS_IOCTL_H_

#include <asm/ioctls.h>

#define TCGETS      _IOR('T', 0x01, struct termios)
#define TCSETS      _IOW('T', 0x02, struct termios)
#define TCSETSW     _IOW('T', 0x03, struct termios)
#define TCSETSF     _IOW('T', 0x04, struct termios)
#define TIOCGPGRP   _IOR('T', 0x05, int)
#define TIOCSPGRP   _IOW('T', 0x06, int)
#define TCFLSH      _IOW('T', 0x07, int)
#define TIOCGWINSZ  _IOR('T', 0x08, struct winsize)
#define TIOCSWINSZ  _IOR('T', 0x09, struct winsize)
#define TIOCGPTN    _IOR('T', 0x30, unsigned int)
#define TIOCSPTLCK  _IOW('T', 0x31, int)
#define TIOCSIG     _IOW('T', 0x36, int)
#define TIOCGRANTPT _IO('T', 0x60)

#ifdef __cplusplus
extern "C"
{
#endif

    int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif /* _IOCTL_H_ */
