#include <lyos/ipc.h>
#include <errno.h>

/* These userspace functions should not be called from the kernel however they
 * are required by liblyos. */

int syscall_entry(int syscall_nr, MESSAGE* m) { return ENOSYS; }

int send_recv(int function, int src_dest, MESSAGE* msg) { return ENOSYS; }
