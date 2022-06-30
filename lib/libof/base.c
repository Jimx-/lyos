#include <lyos/types.h>
#include <lyos/const.h>
#include <errno.h>

#include <libfdt/libfdt.h>
#include "libof.h"

void of_n_addr_size_cells(const void* blob, unsigned long offset, int* addrc,
                          int* sizec)
{
    int parent_offset = fdt_parent_offset(blob, offset);
    int na, ns;
    int na_found = FALSE, ns_found = FALSE;
    const u32* prop;

    while (parent_offset >= 0 && !(na_found && ns_found)) {
        prop = fdt_getprop(blob, parent_offset, "#address-cells", NULL);
        if (prop && !na_found) {
            na = of_read_number(prop, 1);
            na_found = TRUE;
        }

        prop = fdt_getprop(blob, parent_offset, "#size-cells", NULL);
        if (prop && !ns_found) {
            ns = of_read_number(prop, 1);
            ns_found = TRUE;
        }

        parent_offset = fdt_parent_offset(blob, parent_offset);
    }

    if (!na_found) na = 1;
    if (!ns_found) ns = 1;

    if (addrc) *addrc = na;
    if (sizec) *sizec = ns;
}

static int __of_parse_phandle_with_args(const void* blob, unsigned long offset,
                                        const char* list_name,
                                        const char* cells_name, int cell_count,
                                        int index,
                                        struct of_phandle_args* out_args)
{
    struct of_phandle_iterator it;
    int retval, cur_index = 0;

    of_for_each_phandle(&it, retval, blob, offset, list_name, cells_name, -1)
    {
        retval = -ENOENT;
        if (cur_index == index) {
            if (!it.phandle) return retval;

            if (out_args) {
                int c = of_phandle_iterator_args(&it, out_args->args,
                                                 MAX_PHANDLE_ARGS);
                out_args->phandle = it.phandle;
                out_args->offset = it.offset;
                out_args->args_count = c;
            }

            return 0;
        }

        cur_index++;
    }

    return retval;
}

int of_parse_phandle_with_args(const void* blob, unsigned long offset,
                               const char* list_name, const char* cells_name,
                               int index, struct of_phandle_args* out_args)
{
    int cell_count = -1;

    if (index < 0) return -EINVAL;

    if (!cells_name) cell_count = 0;

    return __of_parse_phandle_with_args(blob, offset, list_name, cells_name,
                                        cell_count, index, out_args);
}

int of_property_read_string_helper(const void* blob, unsigned long offset,
                                   const char* propname, const char** out_strs,
                                   size_t sz, int skip)
{
    int len;
    int l = 0, i = 0;
    const char *p, *end;

    p = (const char*)fdt_getprop(blob, offset, propname, &len);
    if (!p) return -EINVAL;
    end = p + len;

    for (i = 0; p < end && (!out_strs || i < skip + sz); i++, p += l) {
        l = strnlen(p, end - p) + 1;
        if (p + l > end) return -EILSEQ;
        if (out_strs && i >= skip) *out_strs++ = p;
    }
    i -= skip;
    return i <= 0 ? -ENODATA : i;
}
