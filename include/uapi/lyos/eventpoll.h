#ifndef _UAPI_LYOS_EVENTPOLL_H_
#define _UAPI_LYOS_EVENTPOLL_H_

#include <lyos/types.h>

#define EPOLLIN     (__poll_t)0x00000001
#define EPOLLPRI    (__poll_t)0x00000002
#define EPOLLOUT    (__poll_t)0x00000004
#define EPOLLERR    (__poll_t)0x00000008
#define EPOLLHUP    (__poll_t)0x00000010
#define EPOLLNVAL   (__poll_t)0x00000020
#define EPOLLRDNORM (__poll_t)0x00000040
#define EPOLLRDBAND (__poll_t)0x00000080
#define EPOLLWRNORM (__poll_t)0x00000100
#define EPOLLWRBAND (__poll_t)0x00000200
#define EPOLLMSG    (__poll_t)0x00000400
#define EPOLLRDHUP  (__poll_t)0x00002000

#endif
