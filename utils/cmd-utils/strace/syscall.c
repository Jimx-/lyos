#include <stdio.h>

#include "proto.h"
#include "syscallent.h"

static const struct syscallent syscallents[] = {
#define SEN(name) trace_##name, #name
#include "syscallents.h"
};
static const size_t nsyscalls = sizeof(syscallents) / sizeof(syscallents[0]);

static int trace_syscall(struct tcb* tcp)
{
    printf("%d", tcp->msg_type_in);

    return RVAL_DECODED;
}

const struct syscallent sysent_stub = {
    .sys_no = 0,
    .sys_func = trace_syscall,
    .sys_name = "syscall",
};

int syscall_trace_entering(struct tcb* tcp, int* sig)
{
    int retval;

    if (tcp->msg_type_in < nsyscalls &&
        syscallents[tcp->msg_type_in].sys_no > 0)
        tcp->sysent = &syscallents[tcp->msg_type_in];
    else
        tcp->sysent = NULL;

    printf("%s(", tcb_sysent(tcp)->sys_name);
    retval = tcb_sysent(tcp)->sys_func(tcp);
    fflush(stdout);

    return retval;
}

int syscall_trace_exiting(struct tcb* tcp)
{
    int retval = 0;
    int out_ret = 0;
    int base = 10;
    int err = 0;

    if (tcp->sys_trace_ret & RVAL_DECODED)
        out_ret = tcp->sys_trace_ret;
    else
        out_ret = tcb_sysent(tcp)->sys_func(tcp);

    printf(") ");

    switch (out_ret & RVAL_MASK) {
    case RVAL_HEX:
    case RVAL_OCTAL:
        base = (out_ret & RVAL_MASK) == RVAL_HEX ? 16 : 8;
        /* fall-through */
    case RVAL_UDECIMAL:
        retval = tcp->msg_out.RETVAL;

        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case RVAL_FD:
        retval = tcp->msg_out.FD;
        if (retval < 0) {
            err = -retval;
            retval = -1;
        }
        break;
    case RVAL_TID:
        retval = tcp->msg_out.PID;
        break;
    case RVAL_SPECIAL:
        switch (tcp->msg_type_in) {
        case GETSETID:
        case UMASK:
            retval = tcp->msg_out.RETVAL;
            break;
        case UTIMENSAT:
        case FCHOWN:
        case FCHOWNAT:
            retval = tcp->msg_out.RETVAL;

            if (retval > 0) {
                err = retval;
                retval = -1;
            }
            break;
        case MMAP:
            base = 16;
            retval = (int)tcp->msg_out.u.m_mm_mmap_reply.retaddr;
            break;
        case SENDTO:
        case RECVFROM:
        case SENDMSG:
        case RECVMSG:
            retval = tcp->msg_out.u.m_vfs_sendrecv.status;
            if (retval < 0) {
                err = -retval;
                retval = -1;
            }
            break;
        }
        break;
    }

    switch (base) {
    case 10:
        printf("= %d", retval);
        break;
    case 16:
        printf("= 0x%x", retval);
        break;
    }

    if (err != 0) {
        putchar(' ');
        print_err(err);
    }

    if ((out_ret & RVAL_STR) && tcp->aux_str) printf(" (%s)", tcp->aux_str);

    printf("\n");

    return 0;
}
