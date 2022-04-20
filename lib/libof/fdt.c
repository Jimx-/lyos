#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/types.h>
#include <errno.h>

#include <libfdt/libfdt.h>
#include "libof.h"

struct phandle_scan_args {
    phandle_t phandle;
    unsigned long offset;
};

int of_scan_fdt(int (*scan)(void*, unsigned long, const char*, int, void*),
                void* arg, void* blob)
{
    int offset, depth = -1;
    int retval = 0;
    const char* pathname;

    for (offset = fdt_next_node(blob, -1, &depth);
         offset >= 0 && depth >= 0 && !retval;
         offset = fdt_next_node(blob, offset, &depth)) {
        pathname = fdt_get_name(blob, offset, NULL);

        retval = scan(blob, offset, pathname, depth, arg);
    }

    return retval;
}

static int fdt_scan_phandle(void* blob, unsigned long offset, const char* name,
                            int depth, void* arg)
{
    struct phandle_scan_args* psa = arg;
    phandle_t handle;
    const uint32_t* phandle = fdt_getprop(blob, offset, "phandle", NULL);

    if (!phandle) return 0;
    handle = be32_to_cpup(phandle);

    if (handle != psa->phandle) return 0;

    psa->offset = offset;
    return 1;
}

int of_find_node_by_phandle(const void* blob, phandle_t handle,
                            unsigned long* offp)
{
    struct phandle_scan_args psa;

    psa.phandle = handle;
    psa.offset = -1;

    of_scan_fdt(fdt_scan_phandle, &psa, (void*)blob);

    if (psa.offset == -1) return -EINVAL;

    *offp = psa.offset;
    return 0;
}

int of_phandle_iterator_init(struct of_phandle_iterator* it, const void* blob,
                             unsigned long offset, const char* list_name,
                             const char* cells_name, int cell_count)
{
    const __be32* list;
    int size;

    memset(it, 0, sizeof(*it));

    if (cell_count < 0 && !cells_name) return -EINVAL;

    list = fdt_getprop(blob, offset, list_name, &size);
    if (!list) return -ENOENT;

    it->cells_name = cells_name;
    it->cell_count = cell_count;
    it->blob = blob;
    it->parent_offset = offset;
    it->list_end = list + size / sizeof(*list);
    it->phandle_end = list;
    it->cur = list;

    return 0;
}

int of_phandle_iterator_next(struct of_phandle_iterator* it)
{
    uint32_t count = 0;
    int ret;

    if (it->offset != -1) {
        it->offset = -1;
    }

    if (!it->cur || it->phandle_end >= it->list_end) return -ENOENT;

    it->cur = it->phandle_end;

    it->phandle = be32_to_cpup(it->cur++);

    if (it->phandle) {
        it->offset = -1;
        ret = of_find_node_by_phandle(it->blob, it->phandle, &it->offset);

        if (it->cells_name) {
            if (ret) return -EINVAL;

            const uint32_t* cell_prop =
                fdt_getprop(it->blob, it->offset, it->cells_name, NULL);

            if (cell_prop) {
                count = be32_to_cpup(cell_prop);
            } else {
                if (it->cell_count >= 0)
                    count = it->cell_count;
                else
                    return -EINVAL;
            }
        } else {
            count = it->cell_count;
        }

        if (it->cur + count > it->list_end) return -EINVAL;
    }

    it->phandle_end = it->cur + count;
    it->cur_count = count;

    return 0;
}

int of_phandle_iterator_args(struct of_phandle_iterator* it, uint32_t* args,
                             int size)
{
    int i, count;

    count = it->cur_count;

    if (size < count) count = size;

    for (i = 0; i < count; i++)
        args[i] = be32_to_cpup(it->cur++);

    return count;
}

static int of_fdt_is_compatible(const void* blob, unsigned long node,
                                const char* compat)
{
    const char* cp;
    int cplen;
    unsigned long l, score = 0;

    cp = fdt_getprop(blob, node, "compatible", &cplen);
    if (cp == NULL) return 0;
    while (cplen > 0) {
        score++;
        if (strncmp(cp, compat, strlen(compat)) == 0) return score;
        l = strlen(cp) + 1;
        cp += l;
        cplen -= l;
    }

    return 0;
}

int of_flat_dt_match(const void* blob, unsigned long node,
                     const char* const* compat)
{
    unsigned int tmp, score = 0;

    if (!compat) return 0;

    while (*compat) {
        tmp = of_fdt_is_compatible(blob, node, *compat);
        if (tmp && (score == 0 || (tmp < score))) score = tmp;
        compat++;
    }

    return score;
}
