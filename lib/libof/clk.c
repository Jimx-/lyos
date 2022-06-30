#include <lyos/types.h>
#include <errno.h>

#include <libfdt/libfdt.h>
#include "libof.h"

int of_parse_clkspec(const void* blob, unsigned long offset, int index,
                     const char* name, struct of_phandle_args* out_args)
{
    int ret = -ENOENT;

    while (offset >= 0) {
        ret = of_parse_phandle_with_args(blob, offset, "clocks", "#clock-cells",
                                         index, out_args);
        if (!ret) break;

        offset = fdt_parent_offset(blob, offset);
        if (offset >= 0 && !fdt_getprop(blob, offset, "clock-ranges", NULL))
            break;
        index = 0;
    }

    return ret;
}
