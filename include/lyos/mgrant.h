#ifndef _LYOS_MGRANT_H_
#define _LYOS_MGRANT_H_

#include <sys/types.h>
#include <lyos/types.h>
#include <lyos/list.h>

typedef struct {
    int flags;
    int seq;

    union {
        struct list_head list;

        struct {
            endpoint_t to;
            vir_bytes addr;
            size_t len;
        } direct;

        struct {
            endpoint_t to;
            endpoint_t from;
            mgrant_id_t grant;
        } indirect;

        struct {
            endpoint_t from;
            endpoint_t to;
            vir_bytes addr;
            size_t len;
        } proxy;
    } u;
} mgrant_t;

#define MGF_READ     0x001
#define MGF_WRITE    0x002
#define MGF_ACCMODES (MGF_READ | MGF_WRITE)
#define MGF_USED     0x004
#define MGF_DIRECT   0x008
#define MGF_INDIRECT 0x010
#define MGF_PROXY    0x020
#define MGF_VALID    0x040

#define GRANT_INVALID ((mgrant_id_t)-1)

#define GRANT_SHIFT        20
#define GRANT_MAX_SEQ      (1 << (31 - GRANT_SHIFT))
#define GRANT_MAX_IDX      (1 << GRANT_SHIFT)
#define GRANT_ID(idx, seq) ((mgrant_id_t)((seq << GRANT_SHIFT) | (idx)))
#define GRANT_SEQ(g)       (((g) >> GRANT_SHIFT) & (GRANT_MAX_SEQ - 1))
#define GRANT_IDX(g)       ((g) & (GRANT_MAX_IDX - 1))

int setgrant(mgrant_t* grants, size_t size);

mgrant_id_t mgrant_set_direct(endpoint_t to, vir_bytes addr, size_t size,
                              int access);
mgrant_id_t mgrant_set_indirect(endpoint_t to, endpoint_t from,
                                mgrant_id_t grant);
mgrant_id_t mgrant_set_proxy(endpoint_t to, endpoint_t from, vir_bytes addr,
                             size_t size, int access);
int mgrant_revoke(mgrant_id_t grant);

#endif
