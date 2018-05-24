#ifndef _SYS_SHM_H_
#define _SYS_SHM_H_

#include <sys/ipc.h>

#define SHM_RDONLY  010000
#define SHM_RND     020000

struct shmid {
    struct ipc_perm perm;
    size_t          size;
    pid_t           lpid;
    pid_t           cpid;
};

#define SHMMNI  1024

#endif
