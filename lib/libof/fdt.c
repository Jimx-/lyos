#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/type.h>

#include <libfdt/libfdt.h>

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
