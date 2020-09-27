#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <lyos/const.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <lyos/mgrant.h>

#define NR_STATIC_GRANTS 10
static mgrant_t static_grants[NR_STATIC_GRANTS];
static mgrant_t* grants;
static size_t grant_size;
static DEF_LIST(free_list);

static void alloc_grants(size_t count)
{
    mgrant_t *new_grants, *g;
    size_t new_size;

    INIT_LIST_HEAD(&free_list);

    if (!grants && count <= NR_STATIC_GRANTS) {
        new_grants = static_grants;
        new_size = NR_STATIC_GRANTS;
    } else {
        if (count) {
            if (count >= GRANT_MAX_IDX) return;

            if (count > GRANT_MAX_IDX - grant_size) {
                count = GRANT_MAX_IDX - grant_size;
            }

            new_size = grant_size + count;
        } else {
            new_size = (1 + grant_size) * 2;
        }

        new_grants = malloc(sizeof(*grants) * new_size);
        if (!new_grants) return;
    }

    if (grant_size > 0)
        memcpy(new_grants, grants, sizeof(*grants) * grant_size);

    for (g = new_grants; g < &new_grants[new_size]; g++) {
        g->flags = 0;
        g->seq = 0;
        list_add(&g->u.list, &free_list);
    }

    if (setgrant(new_grants, new_size) != 0) {
        if (new_grants != static_grants) free(new_grants);
        return;
    }

    if (grants && grants != static_grants) free(grants);
    grants = new_grants;
    grant_size = new_size;
}

static mgrant_t* alloc_slot(void)
{
    mgrant_t* g;

    if (list_empty(&free_list)) {
        alloc_grants(0);
        if (list_empty(&free_list)) {
            errno = ENOSPC;
            return NULL;
        }
    }

    g = list_entry(free_list.next, mgrant_t, u.list);
    list_del(&g->u.list);

    return g;
}

mgrant_id_t mgrant_set_direct(endpoint_t to, vir_bytes addr, size_t size,
                              int access)
{
    mgrant_t* g;

    g = alloc_slot();
    if (!g) return GRANT_INVALID;

    g->u.direct.to = to;
    g->u.direct.addr = addr;
    g->u.direct.len = size;
    g->flags = MGF_USED | MGF_VALID | MGF_DIRECT | (access & MGF_ACCMODES);

    return GRANT_ID(g - grants, g->seq);
}

mgrant_id_t mgrant_set_indirect(endpoint_t to, endpoint_t from,
                                mgrant_id_t grant)
{
    mgrant_t* g;

    g = alloc_slot();
    if (!g) return GRANT_INVALID;

    g->u.indirect.to = to;
    g->u.indirect.from = from;
    g->u.indirect.grant = grant;
    g->flags = MGF_USED | MGF_VALID | MGF_INDIRECT;

    return GRANT_ID(g - grants, g->seq);
}

mgrant_id_t mgrant_set_proxy(endpoint_t to, endpoint_t from, vir_bytes addr,
                             size_t size, int access)
{
    mgrant_t* g;

    g = alloc_slot();
    if (!g) return GRANT_INVALID;

    g->u.proxy.to = to;
    g->u.proxy.from = from;
    g->u.proxy.addr = addr;
    g->u.proxy.len = size;
    g->flags = MGF_USED | MGF_VALID | MGF_PROXY | (access & MGF_ACCMODES);

    return GRANT_ID(g - grants, g->seq);
}

int mgrant_revoke(mgrant_id_t grant)
{
    int idx;
    mgrant_t* g;

    idx = GRANT_IDX(grant);
    g = &grants[idx];

    g->flags = 0;

    if (g->seq < GRANT_MAX_SEQ - 1)
        g->seq++;
    else
        g->seq = 0;

    list_add(&g->u.list, &free_list);

    return 0;
}
