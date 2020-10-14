#ifndef _STRACE_TYPES_H_
#define _STRACE_TYPES_H_

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>

struct tcb {
    int flags;
    int pid;
    MESSAGE msg_in;
    MESSAGE msg_out;

    int msg_type_in;
    int sys_trace_ret;

    const char* aux_str;
};

#define TCB_STARTUP  0x01
#define TCB_ATTACHED 0x02
#define TCB_ENTERING 0x04

#define entering(tcb) ((tcb)->flags & TCB_ENTERING)
#define exiting(tcb)  (!entering(tcb))

/* Format of syscall return values */
#define RVAL_UDECIMAL 000 /* unsigned decimal format */
#define RVAL_HEX      001 /* hex format */
#define RVAL_OCTAL    002 /* octal format */
#define RVAL_FD       010 /* file descriptor */
#define RVAL_TID      011 /* task ID */
#define RVAL_SID      012 /* session ID */
#define RVAL_TGID     013 /* thread group ID */
#define RVAL_PGID     014 /* process group ID */
#define RVAL_MASK     017 /* mask for these values */

#define RVAL_STR  020 /* Print `auxstr' field after return val */
#define RVAL_NONE 040 /* Print nothing */

#define RVAL_DECODED 0100 /* syscall decoding finished */
#define RVAL_IOCTL_DECODED \
    0200 /* ioctl sub-parser successfully decoded the argument */

#endif
