#ifndef _SYS_IPC_H_
#define _SYS_IPC_H_

#include <sys/types.h>
#include <stdint.h>

struct ipc_perm {
    uid_t uid;
    uid_t cuid;
    gid_t gid;
    gid_t cgid;
    mode_t mode;

    uint16_t seq;
    key_t key;
};

#define IPC_R 000400 /* read permission */
#define IPC_W 000200 /* write permission */
#define IPC_M 010000

/* Mode bits for `msgget', `semget', and `shmget'.  */
#define IPC_CREAT  01000 /* Create key if key does not exist. */
#define IPC_EXCL   02000 /* Fail if key exists.  */
#define IPC_NOWAIT 04000 /* Return error on wait.  */

/* Control commands for `msgctl', `semctl', and `shmctl'.  */
#define IPC_RMID 0 /* Remove identifier.  */
#define IPC_SET  1 /* Set `ipc_perm' options.  */
#define IPC_STAT 2 /* Get `ipc_perm' options.  */
#ifdef __USE_GNU
#define IPC_INFO 3 /* See ipcs.  */
#endif

#define IPC_PRIVATE ((key_t)0)

#endif
