#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <lyos/netlink.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "types.h"
#include "xlat.h"
#include "proto.h"

#include "xlat/addrfams.h"

#define SIZEOF_SA_FAMILY sizeof(sa_family_t)

static void print_sockaddr_data_un(struct tcb* tcp, const void* const buf,
                                   size_t addrlen)
{
    const struct sockaddr_un* const sa_un = buf;

    printf("sun_path=");
    if (sa_un->sun_path[0]) {
        printf("\"%s\"", sa_un->sun_path);
    } else {
        printf("\"@%s\"", sa_un->sun_path + 1);
    }
}

static void print_sockaddr_data_in(struct tcb* tcp, const void* const buf,
                                   size_t addrlen)
{
    const struct sockaddr_in* const sa_in = buf;
    char addr_buf[INET_ADDRSTRLEN];

    printf("sin_port=%u, ", ntohs(sa_in->sin_port));
    inet_ntop(AF_INET, &sa_in->sin_addr, addr_buf, sizeof(addr_buf));
    printf("sin_addr=\"%s\"", addr_buf);
}

static void print_sockaddr_data_nl(struct tcb* tcp, const void* const buf,
                                   size_t addrlen)
{
    const struct sockaddr_nl* const sa_nl = buf;

    printf("nl_pid=%u, ", sa_nl->nl_pid);
    printf("nl_groups=0x%lx", sa_nl->nl_groups);
}

typedef void (*const sockaddr_printer)(struct tcb* tcp, const void* const,
                                       size_t);

static const struct {
    const sockaddr_printer printer;
    const int min_len;
} sa_printers[] = {
    [AF_UNIX] = {print_sockaddr_data_un, SIZEOF_SA_FAMILY + 1},
    [AF_INET] = {print_sockaddr_data_in, sizeof(struct sockaddr_in)},
    [AF_NETLINK] = {print_sockaddr_data_nl, SIZEOF_SA_FAMILY + 1},
};

void print_sockaddr(struct tcb* tcp, const void* const buf, const int addrlen)
{
    const struct sockaddr* const sa = buf;

    printf("{sa_family=");
    print_flags(sa->sa_family, &addrfams);

    if (addrlen > SIZEOF_SA_FAMILY) {
        printf(", ");

        if (sa->sa_family < sizeof(sa_printers) / sizeof(sa_printers[0]) &&
            sa_printers[sa->sa_family].printer &&
            addrlen >= sa_printers[sa->sa_family].min_len) {
            sa_printers[sa->sa_family].printer(tcp, buf, addrlen);
        } else {
        }
    }

    printf("}");
}

int decode_sockaddr(struct tcb* tcp, const void* addr, size_t addrlen)
{
    if (addrlen < 2) {
        print_addr((uint64_t)addr);
        return -1;
    }

    union {
        struct sockaddr sa;
        struct sockaddr_storage storage;
        char pad[sizeof(struct sockaddr_storage) + 1];
    } addrbuf;

    if ((unsigned)addrlen > sizeof(addrbuf.storage))
        addrlen = sizeof(addrbuf.storage);

    if (!fetch_mem(tcp, addr, addrbuf.pad, addrlen)) {
        print_addr((uint64_t)addr);
        return -1;
    }

    memset(&addrbuf.pad[addrlen], 0, sizeof(addrbuf.pad) - addrlen);

    print_sockaddr(tcp, &addrbuf, addrlen);

    return addrbuf.sa.sa_family;
}
