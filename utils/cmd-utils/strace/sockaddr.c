#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>

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

typedef void (*const sockaddr_printer)(struct tcb* tcp, const void* const,
                                       size_t);

static const struct {
    const sockaddr_printer printer;
    const int min_len;
} sa_printers[] = {
    [AF_UNIX] = {print_sockaddr_data_un, SIZEOF_SA_FAMILY + 1},
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
