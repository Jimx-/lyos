#ifndef _SYS_IPC_H_
#define _SYS_IPC_H_

#include <sys/types.h>
#include <stdint.h>

struct ipc_perm {
    uid_t   uid;
    uid_t   cuid;
    gid_t   gid;
    gid_t   cgid;
    mode_t  mode;

    uint16_t seq;
    key_t   key;
};

#define IPC_R       000400  /* read permission */
#define IPC_W       000200  /* write permission */
#define IPC_M       010000

#define IPC_CREAT   001000
#define IPC_EXCL    002000
#define IPC_NOWAIT  004000

#define IPC_PRIVATE ((key_t)0)

#endif
