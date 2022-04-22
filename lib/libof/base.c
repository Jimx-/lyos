#include <lyos/types.h>
#include <lyos/const.h>

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
