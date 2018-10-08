#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/type.h>

#include <libfdt/libfdt.h>

PUBLIC int of_scan_fdt(void* blob)
{
    int offset, depth = -1;
    const char* pathname;

    for (offset = fdt_next_node(blob, -1, &depth);
         offset >= 0 && depth >= 0;
         offset = fdt_next_node(blob, offset, &depth)) {
        pathname = fdt_get_name(blob, offset, NULL);
        printk("%s\n", pathname);
    }

    return 0;
}
