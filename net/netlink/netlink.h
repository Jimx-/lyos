#ifndef _NETLINK_NETLINK_H_
#define _NETLINK_NETLINK_H_

#include <sys/types.h>
#include <sys/syslimits.h>
#include <lyos/netlink.h>
#include <lyos/scm.h>

#include <libsockdriver/libsockdriver.h>

struct netlink_skb_params {
    struct scm_creds creds;
    u32 portid;
    u32 dest_group;
};

#define NETLINK_CB(skb)    (*(struct netlink_skb_params*)&((skb)->cb))
#define NETLINK_CREDS(skb) (&NETLINK_CB((skb)).creds)

struct nlsock {
    struct sock sock;
    struct list_head hash;
    struct list_head node;

    u32 portid;
    u32 bound;
    u32 subscriptions;

    bitchunk_t* groups;
    size_t ngroups;

    u32 dst_portid;
    u32 dst_group;
};

#define nls_is_shutdown(nls, mask) sock_is_shutdown(&(nls)->sock, mask)

#define nls_get_protocol(nls) sock_protocol(&(nls)->sock)
#define nls_get_type(nls)     sock_type(&(nls)->sock)

#define NLS_HASH_LOG2 4
#define NLS_HASH_SIZE ((unsigned long)1 << NLS_HASH_LOG2)
#define NLS_HASH_MASK (((unsigned long)1 << NLS_HASH_LOG2) - 1)

struct nltable {
    int groups;
    bitchunk_t* listeners;
    int registered;

    struct list_head hash_table[NLS_HASH_SIZE];
    struct list_head mc_list;
};

static inline struct nlsock* to_nlsock(struct sock* sock)
{
    return list_entry(sock, struct nlsock, sock);
}

#endif
