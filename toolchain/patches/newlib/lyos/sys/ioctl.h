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
#define TIOCSWINSZ  _IOW('T', 0x09, struct winsize)
#define TIOCSCTTY   _IO('T', 0x0E)
#define FIONREAD    _IOR('T', 0x1B, int)
#define TIOCGPTN    _IOR('T', 0x30, unsigned int)
#define TIOCSPTLCK  _IOW('T', 0x31, int)
#define TIOCSIG     _IOW('T', 0x36, int)
#define TIOCGRANTPT _IO('T', 0x60)

#define SIOCGIFCONF    _IOWR(0x89, 0x12, struct ifconf)
#define SIOCGIFFLAGS   _IOWR(0x89, 0x13, struct ifreq)
#define SIOCSIFFLAGS   _IOW(0x89, 0x14, struct ifreq)
#define SIOCGIFADDR    _IOWR(0x89, 0x15, struct ifreq)
#define SIOCSIFADDR    _IOW(0x89, 0x16, struct ifreq)
#define SIOCGIFDSTADDR _IOWR(0x89, 0x17, struct ifreq)
#define SIOCSIFDSTADDR _IOW(0x89, 0x18, struct ifreq)
#define SIOCGIFBRDADDR _IOWR(0x89, 0x19, struct ifreq)
#define SIOCSIFBRDADDR _IOW(0x89, 0x1A, struct ifreq)
#define SIOCGIFNETMASK _IOWR(0x89, 0x1B, struct ifreq)
#define SIOCSIFNETMASK _IOW(0x89, 0x1C, struct ifreq)
#define SIOCGIFMETRIC  _IOWR(0x89, 0x1D, struct ifreq)
#define SIOCSIFMETRIC  _IOW(0x89, 0x1E, struct ifreq)
#define SIOCGIFMTU     _IOWR(0x89, 0x21, struct ifreq)
#define SIOCSIFMTU     _IOW(0x89, 0x22, struct ifreq)
#define SIOCGIFINDEX   _IOWR(0x89, 0x33, struct ifreq)

#ifdef __cplusplus
extern "C"
{
#endif

    int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif /* _IOCTL_H_ */
