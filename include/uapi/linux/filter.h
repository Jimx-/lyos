/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __UAPI_LINUX_FILTER_H__
#define __UAPI_LINUX_FILTER_H__

#include <lyos/types.h>
#include <linux/bpf_common.h>

struct sock_filter {
    __u16 code;
    __u8 jt;
    __u8 jf;
    __u32 k;
};

struct sock_fprog {
    unsigned short len;
    struct sock_filter* filter;
};

#endif
