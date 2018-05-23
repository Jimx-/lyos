#ifndef _SYS_IPC_H_
#define _SYS_IPC_H_

#include <sys/types.h>

struct ipc_perm {
    uid_t   uid;
    gid_t   gid;
    mode_t  mode;

    u16     seq;
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
